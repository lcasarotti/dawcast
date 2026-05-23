#include "IcecastClient.h"
#include <juce_audio_basics/juce_audio_basics.h>

IcecastClient::IcecastClient() = default;

IcecastClient::~IcecastClient()
{
    disconnect();
}

bool IcecastClient::isConnected() const
{
    return connected.load();
}

void IcecastClient::disconnect()
{
    const juce::ScopedLock sl(socketCriticalSection);
    if (socket != nullptr)
    {
        socket->close();
        socket.reset();
    }
    connected.store(false);
}

bool IcecastClient::connect(const Settings& settings, juce::String& errorMessage)
{
    disconnect();
    
    currentSettings = settings;

    // Try HTTP PUT first (standard for Icecast 2.4+)
    if (tryPutConnection(settings, errorMessage))
    {
        connected.store(true);
        return true;
    }

    // Fall back to legacy SOURCE method
    DBG("IcecastClient: PUT failed, falling back to SOURCE. Error: " + errorMessage);
    if (trySourceConnection(settings, errorMessage))
    {
        connected.store(true);
        return true;
    }

    return false;
}

static bool writeAllToSocket(juce::StreamingSocket& sock, const void* data, int size)
{
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    int remaining = size;
    while (remaining > 0)
    {
        int ret = sock.write(ptr, remaining);
        if (ret <= 0)
            return false;
        ptr += ret;
        remaining -= ret;
    }
    return true;
}

bool IcecastClient::tryPutConnection(const Settings& settings, juce::String& errorMessage)
{
    const juce::ScopedLock sl(socketCriticalSection);
    
    socket = std::make_unique<juce::StreamingSocket>();
    if (!socket->connect(settings.host, settings.port, 5000))
    {
        errorMessage = "Connection timeout or host unreachable.";
        socket.reset();
        return false;
    }

    // Build authorization
    juce::String auth = settings.username + ":" + settings.password;
    juce::String base64auth = juce::Base64::toBase64(auth.toRawUTF8(), auth.getNumBytesAsUTF8());

    // Build headers
    juce::String path = settings.mountPoint.startsWith("/") ? settings.mountPoint : "/" + settings.mountPoint;
    juce::String contentType = (settings.format == "ogg") ? "audio/ogg" : "audio/mpeg";

    juce::String headers;
    headers << "PUT " << path << " HTTP/1.1\r\n"
            << "Host: " << settings.host << ":" << settings.port << "\r\n"
            << "Authorization: Basic " << base64auth << "\r\n"
            << "User-Agent: DawCast/1.0\r\n"
            << "Content-Type: " << contentType << "\r\n"
            << "Ice-Public: 1\r\n"
            << "Ice-Name: " << settings.streamName << "\r\n"
            << "Ice-Description: " << settings.streamDescription << "\r\n"
            << "Ice-URL: " << settings.streamUrl << "\r\n"
            << "Ice-Genre: " << settings.streamGenre << "\r\n"
            << "Ice-Bitrate: " << settings.bitrateKbps << "\r\n"
            << "Expect: 100-continue\r\n\r\n";

    if (!writeAllToSocket(*socket, headers.toRawUTF8(), headers.getNumBytesAsUTF8()))
    {
        errorMessage = "Failed to send request headers.";
        socket.reset();
        return false;
    }

    // Read the server's response
    juce::String response = readResponseHeaders(*socket);
    if (response.isEmpty())
    {
        errorMessage = "Server did not respond to PUT handshake.";
        socket.reset();
        return false;
    }

    // Icecast server might return "HTTP/1.1 100 Continue" or "HTTP/1.1 200 OK"
    if (response.containsIgnoreCase("100 Continue") || 
        response.containsIgnoreCase("200 OK") || 
        response.containsIgnoreCase("200 Success"))
    {
        return true;
    }

    // Parse the HTTP status line for error messages
    int firstLineEnd = response.indexOf("\r\n");
    if (firstLineEnd > 0)
        errorMessage = "Server rejected connection: " + response.substring(0, firstLineEnd);
    else
        errorMessage = "Server rejected connection with unknown status.";

    socket.reset();
    return false;
}

bool IcecastClient::trySourceConnection(const Settings& settings, juce::String& errorMessage)
{
    const juce::ScopedLock sl(socketCriticalSection);

    socket = std::make_unique<juce::StreamingSocket>();
    if (!socket->connect(settings.host, settings.port, 5000))
    {
        errorMessage = "Connection timeout or host unreachable on fallback.";
        socket.reset();
        return false;
    }

    juce::String auth = settings.username + ":" + settings.password;
    juce::String base64auth = juce::Base64::toBase64(auth.toRawUTF8(), auth.getNumBytesAsUTF8());

    juce::String path = settings.mountPoint.startsWith("/") ? settings.mountPoint : "/" + settings.mountPoint;
    juce::String contentType = (settings.format == "ogg") ? "audio/ogg" : "audio/mpeg";

    juce::String headers;
    headers << "SOURCE " << path << " ICE/1.0\r\n"
            << "content-type: " << contentType << "\r\n"
            << "authorization: Basic " << base64auth << "\r\n"
            << "ice-name: " << settings.streamName << "\r\n"
            << "ice-url: " << settings.streamUrl << "\r\n"
            << "ice-genre: " << settings.streamGenre << "\r\n"
            << "ice-bitrate: " << settings.bitrateKbps << "\r\n"
            << "ice-public: 1\r\n"
            << "ice-description: " << settings.streamDescription << "\r\n\r\n";

    if (!writeAllToSocket(*socket, headers.toRawUTF8(), headers.getNumBytesAsUTF8()))
    {
        errorMessage = "Failed to send legacy SOURCE headers.";
        socket.reset();
        return false;
    }

    // Read response
    juce::String response = readResponseHeaders(*socket);
    if (response.isEmpty())
    {
        errorMessage = "Server did not respond to legacy SOURCE handshake.";
        socket.reset();
        return false;
    }

    // Legacy response is often "HTTP/1.0 200 OK" or "OK"
    if (response.containsIgnoreCase("200 OK") || response.startsWithIgnoreCase("OK"))
    {
        return true;
    }

    int firstLineEnd = response.indexOf("\r\n");
    if (firstLineEnd > 0)
        errorMessage = "Server rejected connection (SOURCE): " + response.substring(0, firstLineEnd);
    else
        errorMessage = "Server rejected connection with unknown status.";

    socket.reset();
    return false;
}

juce::String IcecastClient::readResponseHeaders(juce::StreamingSocket& sock)
{
    juce::String response;
    char buf[1];
    int bytesRead = 0;
    
    // Read byte by byte until we get the end of headers marker (\r\n\r\n) or time out
    while (bytesRead < 2048)
    {
        // 1-second read timeout
        int ret = sock.read(buf, 1, false);
        if (ret <= 0)
            break;
            
        response += buf[0];
        bytesRead++;
        
        if (response.endsWith("\r\n\r\n"))
            break;
    }
    return response;
}

bool IcecastClient::sendAudioData(const uint8_t* data, int size)
{
    if (!connected.load())
        return false;

    const juce::ScopedLock sl(socketCriticalSection);
    if (socket == nullptr)
        return false;

    return writeAllToSocket(*socket, data, size);
}

void IcecastClient::updateMetadata(const juce::String& artist, const juce::String& title)
{
    if (!connected.load())
        return;

    // Capture required settings values before launching thread
    juce::String host = currentSettings.host;
    int port = currentSettings.port;
    juce::String mount = currentSettings.mountPoint;
    juce::String user = currentSettings.username;
    juce::String pass = currentSettings.password;

    juce::Thread::launch([host, port, mount, user, pass, artist, title]()
    {
        // URL encode metadata string
        juce::String songText = artist;
        if (songText.isNotEmpty() && title.isNotEmpty())
            songText += " - ";
        songText += title;

        juce::String mountPath = mount.startsWith("/") ? mount : "/" + mount;

        // Build Admin metadata URL
        juce::URL url("http://" + host + ":" + juce::String(port) + "/admin/metadata");
        url = url.withParameter("mount", mountPath)
                 .withParameter("mode", "updinfo")
                 .withParameter("song", songText);

        juce::String auth = user + ":" + pass;
        juce::String base64auth = juce::Base64::toBase64(auth.toRawUTF8(), auth.getNumBytesAsUTF8());
        
        juce::String extraHeaders = "User-Agent: DawCast/1.0\r\n"
                                    "Authorization: Basic " + base64auth;

        int statusCode = 0;
        auto stream = std::unique_ptr<juce::InputStream>(
            url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders(extraHeaders)
                    .withConnectionTimeoutMs(4000)
                    .withStatusCode(&statusCode)
            )
        );

        if (stream != nullptr && statusCode == 200)
        {
            DBG("IcecastClient: Metadata updated successfully: " + songText);
        }
        else
        {
            DBG("IcecastClient: Metadata update request returned HTTP status: " << statusCode);
        }
    });
}
