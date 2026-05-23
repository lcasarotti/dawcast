#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Mp3Encoder.h"
#include "OggEncoder.h"
#include <algorithm>

// Constructor
DawCastAudioProcessor::DawCastAudioProcessor()
    : AudioProcessor (BusesProperties()
                      #if ! JucePlugin_IsMidiEffect
                       #if ! JucePlugin_IsSynth
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       #endif
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      #endif
                       ),
      ringBuffer(2, ringBufferSize),
      fifo(ringBufferSize),
      streamingThread(*this)
{
}

// Destructor
DawCastAudioProcessor::~DawCastAudioProcessor()
{
    stopStreaming();
}

const juce::String DawCastAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DawCastAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DawCastAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DawCastAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DawCastAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DawCastAudioProcessor::getNumPrograms()
{
    return 1;
}

int DawCastAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DawCastAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String DawCastAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void DawCastAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void DawCastAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
    
    // Clear ring buffer on play initialization
    ringBuffer.clear();
    fifo.reset();
}

void DawCastAudioProcessor::releaseResources()
{
    stopStreaming();
}

bool DawCastAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void DawCastAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, clear the extra ones
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // We do NOT modify input audio, keeping it pristine for the DAW master chain
    
    // If currently streaming, write audio samples to our ring buffer
    if (isStreaming.load() && totalNumInputChannels > 0)
    {
        int numSamples = buffer.getNumSamples();
        int numChans = std::min(totalNumInputChannels, 2);
        
        int start1, block1, start2, block2;
        fifo.prepareToWrite(numSamples, start1, block1, start2, block2);
        
        if (block1 > 0)
        {
            // Copy channels (up to stereo)
            for (int ch = 0; ch < 2; ++ch)
            {
                int sourceCh = (ch < numChans) ? ch : 0;
                ringBuffer.copyFrom(ch, start1, buffer, sourceCh, 0, block1);
            }
            
            if (block2 > 0)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    int sourceCh = (ch < numChans) ? ch : 0;
                    ringBuffer.copyFrom(ch, start2, buffer, sourceCh, block1, block2);
                }
            }
            
            fifo.finishedWrite(block1 + block2);
            
            // Wake up background streaming thread
            streamingThread.notify();
        }
    }
}

bool DawCastAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* DawCastAudioProcessor::createEditor()
{
    return new DawCastAudioProcessorEditor (*this);
}

void DawCastAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("DawCastSettings");
    
    xml.setAttribute ("host", currentSettings.host);
    xml.setAttribute ("port", currentSettings.port);
    xml.setAttribute ("username", currentSettings.username);
    xml.setAttribute ("password", currentSettings.password);
    xml.setAttribute ("mountPoint", currentSettings.mountPoint);
    xml.setAttribute ("format", currentSettings.format);
    xml.setAttribute ("bitrate", currentSettings.bitrateKbps);
    xml.setAttribute ("targetSampleRate", currentSettings.targetSampleRate);
    
    xml.setAttribute ("streamName", currentSettings.streamName);
    xml.setAttribute ("streamUrl", currentSettings.streamUrl);
    xml.setAttribute ("streamDescription", currentSettings.streamDescription);
    xml.setAttribute ("streamGenre", currentSettings.streamGenre);
    
    xml.setAttribute ("artist", currentSettings.currentArtist);
    xml.setAttribute ("title", currentSettings.currentTitle);

    copyXmlToBinary (xml, destData);
}

void DawCastAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName ("DawCastSettings"))
        {
            currentSettings.host = xmlState->getStringAttribute ("host", currentSettings.host);
            currentSettings.port = xmlState->getIntAttribute ("port", currentSettings.port);
            currentSettings.username = xmlState->getStringAttribute ("username", currentSettings.username);
            currentSettings.password = xmlState->getStringAttribute ("password", currentSettings.password);
            currentSettings.mountPoint = xmlState->getStringAttribute ("mountPoint", currentSettings.mountPoint);
            currentSettings.format = xmlState->getStringAttribute ("format", currentSettings.format);
            currentSettings.bitrateKbps = xmlState->getIntAttribute ("bitrate", currentSettings.bitrateKbps);
            currentSettings.targetSampleRate = xmlState->getIntAttribute ("targetSampleRate", currentSettings.targetSampleRate);
            
            currentSettings.streamName = xmlState->getStringAttribute ("streamName", currentSettings.streamName);
            currentSettings.streamUrl = xmlState->getStringAttribute ("streamUrl", currentSettings.streamUrl);
            currentSettings.streamDescription = xmlState->getStringAttribute ("streamDescription", currentSettings.streamDescription);
            currentSettings.streamGenre = xmlState->getStringAttribute ("streamGenre", currentSettings.streamGenre);
            
            currentSettings.currentArtist = xmlState->getStringAttribute ("artist", currentSettings.currentArtist);
            currentSettings.currentTitle = xmlState->getStringAttribute ("title", currentSettings.currentTitle);
        }
    }
}

// --- Custom DawCast Interfaces ---

void DawCastAudioProcessor::addListener(Listener* listener)
{
    listeners.add(listener);
}

void DawCastAudioProcessor::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

bool DawCastAudioProcessor::isCurrentlyStreaming() const
{
    return isStreaming.load();
}

bool DawCastAudioProcessor::isCurrentlyConnecting() const
{
    return isConnecting.load();
}

void DawCastAudioProcessor::startStreaming(const ConnectionSettings& settings)
{
    if (isStreaming.load() || isConnecting.load())
        return;

    isConnecting.store(true);
    currentSettings = settings;
    listeners.call(&Listener::connectionStateChanged, false, true);

    // Launch connection asynchronously so we do not block the message thread
    juce::Thread::launch([this, settings]()
    {
        handleAsyncConnect(settings);
    });
}

void DawCastAudioProcessor::handleAsyncConnect(const ConnectionSettings& settings)
{
    juce::String errorMessage;
    
    // 1. Instantiate encoder
    std::unique_ptr<AudioEncoder> encoder;
    if (settings.format == "ogg")
        encoder = std::make_unique<OggEncoder>();
    else
        encoder = std::make_unique<Mp3Encoder>();

    double sampleRate = getSampleRate();
    if (sampleRate <= 0)
        sampleRate = 44100.0;
        
    int targetRate = settings.targetSampleRate;
    if (targetRate <= 0)
        targetRate = static_cast<int>(sampleRate);

    // 2. Initialize encoder
    if (!encoder->initialize(sampleRate, 2, targetRate, settings.bitrateKbps))
    {
        isConnecting.store(false);
        juce::MessageManager::callAsync([this]() {
            listeners.call(&Listener::connectionFailed, "Failed to initialize audio encoder.");
            listeners.call(&Listener::connectionStateChanged, false, false);
        });
        return;
    }

    // 3. Establish TCP/HTTP PUT connection to Icecast server
    IcecastClient::Settings clientSettings;
    clientSettings.host = settings.host;
    clientSettings.port = settings.port;
    clientSettings.username = settings.username;
    clientSettings.password = settings.password;
    clientSettings.mountPoint = settings.mountPoint;
    clientSettings.format = settings.format;
    clientSettings.streamName = settings.streamName;
    clientSettings.streamUrl = settings.streamUrl;
    clientSettings.streamDescription = settings.streamDescription;
    clientSettings.streamGenre = settings.streamGenre;
    clientSettings.bitrateKbps = settings.bitrateKbps;

    if (!client.connect(clientSettings, errorMessage))
    {
        isConnecting.store(false);
        juce::MessageManager::callAsync([this, errorMessage]() {
            listeners.call(&Listener::connectionFailed, "Connection failed: " + errorMessage);
            listeners.call(&Listener::connectionStateChanged, false, false);
        });
        return;
    }

    // 4. Success! Enable streaming state
    activeEncoder = std::move(encoder);
    
    // Reset ring buffer trackers
    fifo.reset();

    isStreaming.store(true);
    isConnecting.store(false);

    // Start background encoder thread
    streamingThread.startThread();

    // Update metadata immediately on connection
    if (settings.currentArtist.isNotEmpty() || settings.currentTitle.isNotEmpty())
    {
        client.updateMetadata(settings.currentArtist, settings.currentTitle);
    }

    juce::MessageManager::callAsync([this]() {
        listeners.call(&Listener::connectionStateChanged, true, false);
    });
}

void DawCastAudioProcessor::stopStreaming()
{
    if (!isStreaming.load() && !isConnecting.load())
        return;

    isStreaming.store(false);
    isConnecting.store(false);

    // Stop background thread immediately
    streamingThread.signalThreadShouldExit();
    streamingThread.notify();
    streamingThread.stopThread(2000);

    // Disconnect client socket
    client.disconnect();

    // Reclaim encoder resources
    activeEncoder.reset();

    juce::MessageManager::callAsync([this]() {
        listeners.call(&Listener::connectionStateChanged, false, false);
    });
}

void DawCastAudioProcessor::updateLiveMetadata(const juce::String& artist, const juce::String& title)
{
    currentSettings.currentArtist = artist;
    currentSettings.currentTitle = title;
    
    if (isStreaming.load())
    {
        client.updateMetadata(artist, title);
    }
}

// --- StreamingThread Implementation ---

DawCastAudioProcessor::StreamingThread::StreamingThread(DawCastAudioProcessor& proc)
    : Thread("DawCast Streaming Thread"),
      processor(proc),
      scratchBuffer(2, chunkSize)
{
    encodedData.resize(chunkSize * 2 + 7200); // 7200 extra space as buffer headroom
}

DawCastAudioProcessor::StreamingThread::~StreamingThread()
{
    stopThread(2000);
}

void DawCastAudioProcessor::StreamingThread::run()
{
    while (!threadShouldExit())
    {
        // Sleep until notified or 50ms pass
        wait(50);

        if (threadShouldExit())
            break;

        if (!processor.isStreaming.load())
            continue;

        int numReady = processor.fifo.getNumReady();
        while (numReady >= chunkSize && !threadShouldExit())
        {
            int start1, block1, start2, block2;
            processor.fifo.prepareToRead(chunkSize, start1, block1, start2, block2);

            if (block1 > 0)
            {
                // Copy block1 into scratch
                for (int ch = 0; ch < 2; ++ch)
                {
                    scratchBuffer.copyFrom(ch, 0, processor.ringBuffer, ch, start1, block1);
                }

                // Copy block2 wrap-around
                if (block2 > 0)
                {
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        scratchBuffer.copyFrom(ch, block1, processor.ringBuffer, ch, start2, block2);
                    }
                }

                // Acknowledge read completion
                processor.fifo.finishedRead(block1 + block2);

                // Encode block and write to stream
                if (processor.isStreaming.load() && processor.activeEncoder != nullptr)
                {
                    const float* const* channelPointers = scratchBuffer.getArrayOfReadPointers();
                    int bytesEncoded = processor.activeEncoder->encode(channelPointers, chunkSize,
                                                                      encodedData.data(), static_cast<int>(encodedData.size()));
                    if (bytesEncoded > 0)
                    {
                        if (!processor.client.sendAudioData(encodedData.data(), bytesEncoded))
                        {
                            DBG("StreamingThread: Send audio data failed. Disconnecting...");
                            processor.stopStreaming();
                            break;
                        }
                    }
                    else if (bytesEncoded < 0)
                    {
                        DBG("StreamingThread: Encode error. Disconnecting...");
                        processor.stopStreaming();
                        break;
                    }
                }
            }

            numReady = processor.fifo.getNumReady();
        }
    }
}

// Entry point creation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DawCastAudioProcessor();
}
