#pragma once

#include "AudioEncoder.h"

// Forward declaration of LAME structure to avoid exposing it directly
struct lame_global_struct;

class Mp3Encoder : public AudioEncoder
{
public:
    Mp3Encoder();
    ~Mp3Encoder() override;

    bool initialize(double sampleRate, int numChannels, int targetSampleRate, int bitrateKbps) override;
    int encode(const float* const* inputChannelData, int numSamples, uint8_t* outputBuffer, int maxOutputSize) override;
    int flush(uint8_t* outputBuffer, int maxOutputSize) override;
    const char* getFormatName() const override { return "mp3"; }
    int getBitrate() const override { return bitrate; }

private:
    struct lame_global_struct* gfp = nullptr;
    int bitrate = 128;
    bool initialized = false;
};
