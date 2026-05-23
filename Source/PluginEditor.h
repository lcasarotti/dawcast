#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class AccessibleTextEditor : public juce::TextEditor
{
public:
    AccessibleTextEditor (const juce::String& name = {}) : juce::TextEditor (name) {}

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::backspaceKey)
        {
            auto text = getText();
            auto caretPos = getCaretPosition();
            auto highlight = getHighlightedRegion();

            if (! highlight.isEmpty())
            {
                juce::AccessibilityHandler::postAnnouncement ("selezione cancellata", juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            else if (caretPos > 0 && caretPos <= text.length())
            {
                auto deletedChar = text.substring (caretPos - 1, caretPos);
                if (deletedChar.isNotEmpty())
                {
                    juce::String announcement = deletedChar;
                    if (getPasswordCharacter() != 0)
                        announcement = "punto";
                    else if (deletedChar == " ")
                        announcement = "spazio";

                    juce::AccessibilityHandler::postAnnouncement (announcement, juce::AccessibilityHandler::AnnouncementPriority::high);
                }
            }
        }
        else if (key.getKeyCode() == juce::KeyPress::deleteKey)
        {
            auto text = getText();
            auto caretPos = getCaretPosition();
            auto highlight = getHighlightedRegion();

            if (! highlight.isEmpty())
            {
                juce::AccessibilityHandler::postAnnouncement ("selezione cancellata", juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            else if (caretPos >= 0 && caretPos < text.length())
            {
                auto deletedChar = text.substring (caretPos, caretPos + 1);
                if (deletedChar.isNotEmpty())
                {
                    juce::String announcement = deletedChar;
                    if (getPasswordCharacter() != 0)
                        announcement = "punto";
                    else if (deletedChar == " ")
                        announcement = "spazio";

                    juce::AccessibilityHandler::postAnnouncement (announcement, juce::AccessibilityHandler::AnnouncementPriority::high);
                }
            }
        }

        return juce::TextEditor::keyPressed (key);
    }
};

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
    
    AccessibleTextEditor hostInput;
    juce::Label hostLabel;
    
    AccessibleTextEditor portInput;
    juce::Label portLabel;
    
    AccessibleTextEditor userInput;
    juce::Label userLabel;
    
    AccessibleTextEditor passInput;
    juce::Label passLabel;
    
    AccessibleTextEditor mountInput;
    juce::Label mountLabel;

    // Right Column: Stream Info & Metadata
    juce::Label streamHeader;
    
    AccessibleTextEditor nameInput;
    juce::Label nameLabel;
    
    AccessibleTextEditor genreInput;
    juce::Label genreLabel;
    
    AccessibleTextEditor descInput;
    juce::Label descLabel;
    
    AccessibleTextEditor urlInput;
    juce::Label urlLabel;
    
    AccessibleTextEditor artistInput;
    juce::Label artistLabel;
    
    AccessibleTextEditor titleInput;
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
