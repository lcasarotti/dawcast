#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

#if JUCE_WINDOWS
 #define NOMINMAX
 #include <windows.h>
 #include <oleauto.h>
#endif

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
                postAccessibleAnnouncement ("selezione cancellata");
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

                    postAccessibleAnnouncement (announcement);
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
                postAccessibleAnnouncement ("selezione cancellata");
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

                    postAccessibleAnnouncement (announcement);
                }
            }
        }

        return juce::TextEditor::keyPressed (key);
    }

private:
    void postAccessibleAnnouncement (const juce::String& text)
    {
       #if JUCE_WINDOWS
        if (auto* handler = getAccessibilityHandler())
        {
            if (auto* nativeHandle = handler->getNativeImplementation())
            {
                static HMODULE hUiaDll = GetModuleHandleA ("UIAutomationCore.dll");
                if (hUiaDll == nullptr)
                    hUiaDll = LoadLibraryA ("UIAutomationCore.dll");

                if (hUiaDll != nullptr)
                {
                    typedef HRESULT (WINAPI* UiaRaiseNotificationEventFunc) (
                        IUnknown*,
                        int, // NotificationKind
                        int, // NotificationProcessing
                        BSTR,
                        BSTR
                    );

                    static auto uiaRaiseNotificationEvent = (UiaRaiseNotificationEventFunc)
                        GetProcAddress (hUiaDll, "UiaRaiseNotificationEvent");

                    if (uiaRaiseNotificationEvent != nullptr)
                    {
                        IUnknown* unk = (IUnknown*) nativeHandle;
                        
                        static const GUID uuid_IRawElementProviderSimple = 
                            { 0xd6dd68d1, 0x86fd, 0x4332, { 0x86, 0x66, 0x9a, 0xbe, 0xde, 0xa2, 0xd2, 0x4c } };
                        
                        IUnknown* provider = nullptr;
                        if (SUCCEEDED (unk->QueryInterface (uuid_IRawElementProviderSimple, (void**) &provider)) && provider != nullptr)
                        {
                            BSTR displayString = SysAllocString (text.toWideCharPointer());
                            BSTR activityId = SysAllocString (L"DawCastDeletionAnnouncement");

                            // UIA Notification Kind: 2 (ActionCompleted)
                            // UIA Notification Processing: 0 (ImportantAll)
                            uiaRaiseNotificationEvent (provider, 2, 0, displayString, activityId);

                            SysFreeString (displayString);
                            SysFreeString (activityId);
                            provider->Release();
                            return;
                        }
                    }
                }
            }
        }
       #endif

        juce::AccessibilityHandler::postAnnouncement (text, juce::AccessibilityHandler::AnnouncementPriority::high);
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
