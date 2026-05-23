#include "Mp3Encoder.h"
#include <lame.h>
#include <algorithm>

Mp3Encoder::Mp3Encoder() = default;

Mp3Encoder::~Mp3Encoder()
{
    if (gfp != nullptr)
    {
        lame_close(gfp);
    }
}

bool Mp3Encoder::initialize(double sampleRate, int numChannels, int targetSampleRate, int bitrateKbps)
{
    if (initialized)
    {
        if (gfp != nullptr)
        {
            lame_close(gfp);
            gfp = nullptr;
        }
        initialized = false;
    }

    gfp = lame_init();
    if (gfp == nullptr)
        return false;

    bitrate = bitrateKbps;

    lame_set_in_samplerate(gfp, static_cast<int>(sampleRate));
    lame_set_num_channels(gfp, std::min(numChannels, 2));
    lame_set_out_samplerate(gfp, targetSampleRate);
    
    // Configure quality and bitrate
    lame_set_preset(gfp, bitrateKbps);
    lame_set_bWriteVbrTag(gfp, 0); // No VBR tag for live streaming
    lame_set_VBR(gfp, vbr_off);
    lame_set_brate(gfp, bitrateKbps);
    
    if (numChannels == 1)
    {
        lame_set_mode(gfp, MONO);
    }
    else
    {
        lame_set_mode(gfp, JOINT_STEREO);
    }
    
    lame_set_original(gfp, 0);
    lame_set_error_protection(gfp, 1);
    lame_set_extension(gfp, 0);
    lame_set_strict_ISO(gfp, 0);

    if (lame_init_params(gfp) < 0)
    {
        lame_close(gfp);
        gfp = nullptr;
        return false;
    }

    initialized = true;
    return true;
}

int Mp3Encoder::encode(const float* const* inputChannelData, int numSamples, uint8_t* outputBuffer, int maxOutputSize)
{
    if (!initialized || gfp == nullptr)
        return -1;

    const float* left = inputChannelData[0];
    // If input is mono, feed left channel to both LAME inputs
    const float* right = (inputChannelData[1] != nullptr) ? inputChannelData[1] : left;

    int bytesEncoded = lame_encode_buffer_ieee_float(gfp, 
                                                     left, 
                                                     right, 
                                                     numSamples, 
                                                     outputBuffer, 
                                                     maxOutputSize);
    return bytesEncoded;
}

int Mp3Encoder::flush(uint8_t* outputBuffer, int maxOutputSize)
{
    if (!initialized || gfp == nullptr)
        return -1;

    int bytesFlushed = lame_encode_flush(gfp, outputBuffer, maxOutputSize);
    return bytesFlushed;
}
