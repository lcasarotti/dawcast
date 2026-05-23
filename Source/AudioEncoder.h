#pragma once

#include <cstdint>

class AudioEncoder
{
public:
    virtual ~AudioEncoder() = default;

    /**
     * Initializes the encoder.
     * @param sampleRate The input sample rate from the DAW (e.g. 44100, 48000, 96000).
     * @param numChannels Number of input channels (normally 2).
     * @param targetSampleRate The sample rate to output to the stream (e.g. 44100).
     * @param bitrateKbps The target bitrate in kbps (e.g. 128, 192, 320).
     * @return True if initialization succeeded, false otherwise.
     */
    virtual bool initialize(double sampleRate, int numChannels, int targetSampleRate, int bitrateKbps) = 0;

    /**
     * Encodes a block of stereo floating-point audio.
     * @param inputChannelData Array of channel float pointers.
     * @param numSamples Number of samples per channel.
     * @param outputBuffer Pointer to output buffer where encoded bytes will be written.
     * @param maxOutputSize Maximum size of the output buffer.
     * @return The number of bytes written to the output buffer, or negative on error.
     */
    virtual int encode(const float* const* inputChannelData, int numSamples, uint8_t* outputBuffer, int maxOutputSize) = 0;

    /**
     * Flushes any remaining bytes from the encoder's internal buffers.
     * @param outputBuffer Pointer to output buffer where flushed bytes will be written.
     * @param maxOutputSize Maximum size of the output buffer.
     * @return The number of bytes written, or negative on error.
     */
    virtual int flush(uint8_t* outputBuffer, int maxOutputSize) = 0;

    /**
     * Returns the name of the format ("mp3" or "ogg").
     */
    virtual const char* getFormatName() const = 0;

    /**
     * Returns the actual bitrate being used by the encoder.
     */
    virtual int getBitrate() const = 0;
};
