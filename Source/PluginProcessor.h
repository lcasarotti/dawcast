#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "IcecastClient.h"
#include "AudioEncoder.h"
#include <atomic>
#include <memory>

class DawCastAudioProcessor : public juce::AudioProcessor
{
public:
    DawCastAudioProcessor();
    ~DawCastAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Custom DawCast Interfaces ---

    struct ConnectionSettings
    {
        juce::String host = "127.0.0.1";
        int port = 8000;
        juce::String username = "source";
        juce::String password;
        juce::String mountPoint = "/live.mp3";
        juce::String format = "mp3"; // "mp3" or "ogg"
        int bitrateKbps = 128;
        int targetSampleRate = 44100;

        juce::String streamName = "My DAW Stream";
        juce::String streamUrl = "http://localhost:8000";
        juce::String streamDescription = "Streaming live from DAW";
        juce::String streamGenre = "Live";
        
        juce::String currentArtist;
        juce::String currentTitle;
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void connectionStateChanged(bool connected, bool connecting) = 0;
        virtual void connectionFailed(const juce::String& errorMessage) = 0;
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    void startStreaming(const ConnectionSettings& settings);
    void stopStreaming();
    bool isCurrentlyStreaming() const;
    bool isCurrentlyConnecting() const;
    
    const ConnectionSettings& getConnectionSettings() const { return currentSettings; }
    void updateLiveMetadata(const juce::String& artist, const juce::String& title);

private:
    ConnectionSettings currentSettings;
    IcecastClient client;
    std::unique_ptr<AudioEncoder> activeEncoder;
    
    std::atomic<bool> isStreaming { false };
    std::atomic<bool> isConnecting { false };

    // Circular FIFO buffer variables
    static constexpr int ringBufferSize = 65536;
    juce::AudioBuffer<float> ringBuffer;
    juce::AbstractFifo fifo;

    // Background streaming thread class
    class StreamingThread : public juce::Thread
    {
    public:
        StreamingThread(DawCastAudioProcessor& processor);
        ~StreamingThread() override;

        void run() override;

    private:
        DawCastAudioProcessor& processor;
        static constexpr int chunkSize = 1024;
        juce::AudioBuffer<float> scratchBuffer;
        std::vector<uint8_t> encodedData;
    };

    StreamingThread streamingThread;
    juce::ListenerList<Listener> listeners;

    void handleAsyncConnect(const ConnectionSettings& settings);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DawCastAudioProcessor)
};
