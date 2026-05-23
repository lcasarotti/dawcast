#include "OggEncoder.h"
#include <cstring>
#include <cstdlib>

OggEncoder::OggEncoder() = default;

OggEncoder::~OggEncoder()
{
    if (initialized)
    {
        ogg_stream_clear(&os);
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
    }
}

bool OggEncoder::initialize(double sampleRate, int numChannels, int targetSampleRate, int bitrateKbps)
{
    if (initialized)
    {
        ogg_stream_clear(&os);
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
        initialized = false;
    }

    bitrate = bitrateKbps;
    headersSent = false;

    vorbis_info_init(&vi);

    // Initialize Vorbis encoder in VBR or managed bitrate mode.
    // Icecast streaming works best with managed constant bitrate or high quality VBR.
    // We use vorbis_encode_init with min/max/nominal bitrates set to the same target to simulate CBR.
    long br = static_cast<long>(bitrateKbps) * 1000;
    int ret = vorbis_encode_init(&vi, 2, static_cast<long>(sampleRate), br, br, br);
    if (ret != 0)
    {
        vorbis_info_clear(&vi);
        return false;
    }

    vorbis_comment_init(&vc);
    vorbis_comment_add_tag(&vc, "ENCODER", "DawCast");

    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);
    
    // Initialize stream with a random serial number
    ogg_stream_init(&os, std::rand());

    // Generate header packets
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
    
    // Queue header packets
    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);

    initialized = true;
    return true;
}

int OggEncoder::writePageToBuffer(ogg_page* og, uint8_t* outputBuffer, int bytesWritten, int maxOutputSize)
{
    if (og == nullptr || outputBuffer == nullptr)
        return bytesWritten;

    if (bytesWritten + og->header_len + og->body_len <= maxOutputSize)
    {
        std::memcpy(outputBuffer + bytesWritten, og->header, og->header_len);
        bytesWritten += og->header_len;
        std::memcpy(outputBuffer + bytesWritten, og->body, og->body_len);
        bytesWritten += og->body_len;
    }
    return bytesWritten;
}

int OggEncoder::encode(const float* const* inputChannelData, int numSamples, uint8_t* outputBuffer, int maxOutputSize)
{
    if (!initialized)
        return -1;

    int bytesWritten = 0;

    // First flush the header pages if not done yet
    if (!headersSent)
    {
        ogg_page og;
        while (ogg_stream_flush(&os, &og) > 0)
        {
            bytesWritten = writePageToBuffer(&og, outputBuffer, bytesWritten, maxOutputSize);
        }
        headersSent = true;
    }

    // Submit samples to vorbis analysis buffer
    float** buffer = vorbis_analysis_buffer(&vd, numSamples);
    if (buffer == nullptr)
        return -1;

    const float* left = inputChannelData[0];
    const float* right = (inputChannelData[1] != nullptr) ? inputChannelData[1] : left;

    for (int i = 0; i < numSamples; ++i)
    {
        buffer[0][i] = left[i];
        buffer[1][i] = right[i];
    }

    vorbis_analysis_wrote(&vd, numSamples);

    // Pull blocks and pack into stream packets
    while (vorbis_analysis_blockout(&vd, &vb) == 1)
    {
        vorbis_analysis(&vb, nullptr);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op) == 1)
        {
            ogg_stream_packetin(&os, &op);

            ogg_page og;
            // Write pages as they become full
            while (ogg_stream_pageout(&os, &og) > 0)
            {
                bytesWritten = writePageToBuffer(&og, outputBuffer, bytesWritten, maxOutputSize);
            }
        }
    }

    return bytesWritten;
}

int OggEncoder::flush(uint8_t* outputBuffer, int maxOutputSize)
{
    if (!initialized)
        return -1;

    // Signal end of stream
    vorbis_analysis_wrote(&vd, 0);

    int bytesWritten = 0;
    while (vorbis_analysis_blockout(&vd, &vb) == 1)
    {
        vorbis_analysis(&vb, nullptr);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op) == 1)
        {
            ogg_stream_packetin(&os, &op);
        }
    }

    // Flush any remaining pages
    ogg_page og;
    while (ogg_stream_flush(&os, &og) > 0)
    {
        bytesWritten = writePageToBuffer(&og, outputBuffer, bytesWritten, maxOutputSize);
    }

    return bytesWritten;
}
