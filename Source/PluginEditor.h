#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class DawCastAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                     public DawCastAudioProcessor::Listener,
                                     public juce::Timer
{
public:
    DawCastAudioProcessorEditor (DawCastAudioProcessor&);
    ~DawCastAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // --- Listener callbacks ---
    void connectionStateChanged(bool connected, bool connecting) override;
    void connectionFailed(const juce::String& errorMessage) override;

    // --- Timer callback ---
    void timerCallback() override;

private:
    DawCastAudioProcessor& audioProcessor;

    // --- UI Controls ---

    // Title & Status Header
    juce::Label titleLabel;
    juce::Label statusLabel;

    // Presets UI Controls
    juce::ComboBox presetCombo;
    juce::Label presetLabel;
    juce::TextButton presetSaveButton;
    juce::TextButton presetDeleteButton;

    // Left Column: Audio Options
    juce::Label encoderHeader;
    
    juce::ComboBox formatCombo;
    juce::Label formatLabel;
    
    juce::ComboBox bitrateCombo;
    juce::Label bitrateLabel;
    
    juce::ComboBox sampleRateCombo;
    juce::Label sampleRateLabel;

    // Middle Column: Server details
    juce::Label serverHeader;
    
    juce::TextEditor hostInput;
    juce::Label hostLabel;
    
    juce::TextEditor portInput;
    juce::Label portLabel;
    
    juce::TextEditor userInput;
    juce::Label userLabel;
    
    juce::TextEditor passInput;
    juce::Label passLabel;
    
    juce::TextEditor mountInput;
    juce::Label mountLabel;

    // Right Column: Stream Info & Metadata
    juce::Label streamHeader;
    
    juce::TextEditor nameInput;
    juce::Label nameLabel;
    
    juce::TextEditor genreInput;
    juce::Label genreLabel;
    
    juce::TextEditor descInput;
    juce::Label descLabel;
    
    juce::TextEditor urlInput;
    juce::Label urlLabel;
    
    juce::TextEditor artistInput;
    juce::Label artistLabel;
    
    juce::TextEditor titleInput;
    juce::Label titleLabelField;

    // Footer Controls
    juce::TextButton connectButton;
    juce::TextButton updateMetadataButton;

    // --- Setup Helpers ---
    void setupUIComponents();
    void updateUIState();
    void saveSettingsToProcessor();
    void loadSettingsFromProcessor();
    void updatePresetsCombo();
    void savePresetClicked();
    void deletePresetClicked();
    void userModifiedSettings();
    bool isUpdatingFromPreset = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DawCastAudioProcessorEditor)
};
