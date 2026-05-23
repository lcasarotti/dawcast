#pragma once

#include "AudioEncoder.h"
#include <vorbis/vorbisenc.h>

class OggEncoder : public AudioEncoder
{
public:
    OggEncoder();
    ~OggEncoder() override;

    bool initialize(double sampleRate, int numChannels, int targetSampleRate, int bitrateKbps) override;
    int encode(const float* const* inputChannelData, int numSamples, uint8_t* outputBuffer, int maxOutputSize) override;
    int flush(uint8_t* outputBuffer, int maxOutputSize) override;
    const char* getFormatName() const override { return "ogg"; }
    int getBitrate() const override { return bitrate; }

private:
    ogg_stream_state os;
    ogg_packet op;
    vorbis_info vi;
    vorbis_comment vc;
    vorbis_dsp_state vd;
    vorbis_block vb;

    int bitrate = 128;
    bool initialized = false;
    bool headersSent = false;

    int writePageToBuffer(ogg_page* og, uint8_t* outputBuffer, int bytesWritten, int maxOutputSize);
};
