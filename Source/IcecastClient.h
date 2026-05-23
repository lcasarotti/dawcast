#pragma once

#include <juce_core/juce_core.h>

class IcecastClient
{
public:
    IcecastClient();
    ~IcecastClient();

    struct Settings
    {
        juce::String host;
        int port = 8000;
        juce::String username = "source";
        juce::String password;
        juce::String mountPoint = "/stream.mp3";
        juce::String format = "mp3"; // "mp3" or "ogg"

        juce::String streamName = "DawCast Stream";
        juce::String streamUrl;
        juce::String streamDescription;
        juce::String streamGenre;
        int bitrateKbps = 128;
    };

    /**
     * Connects to the Icecast server using the specified settings.
     * Tries HTTP PUT first, falls back to legacy SOURCE.
     * Runs synchronously, so should be called from a background thread.
     */
    bool connect(const Settings& settings, juce::String& errorMessage);

    /**
     * Closes the connection to the Icecast server.
     */
    void disconnect();

    /**
     * Returns true if currently connected.
     */
    bool isConnected() const;

    /**
     * Sends raw encoded bytes to the server socket.
     * Blocks if socket is full. Returns false if connection drops.
     */
    bool sendAudioData(const uint8_t* data, int size);

    /**
     * Sends an out-of-band metadata update (artist and title) via an asynchronous HTTP GET request.
     */
    void updateMetadata(const juce::String& artist, const juce::String& title);

private:
    Settings currentSettings;
    std::unique_ptr<juce::StreamingSocket> socket;
    mutable juce::CriticalSection socketCriticalSection;
    std::atomic<bool> connected { false };

    bool tryPutConnection(const Settings& settings, juce::String& errorMessage);
    bool trySourceConnection(const Settings& settings, juce::String& errorMessage);
    juce::String readResponseHeaders(juce::StreamingSocket& sock);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IcecastClient)
};
