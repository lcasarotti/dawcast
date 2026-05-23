#include "PluginProcessor.h"
#include "PluginEditor.h"

DawCastAudioProcessorEditor::DawCastAudioProcessorEditor (DawCastAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    
    // 1. Establish plugin window dimensions
    setSize (820, 560);

    // 2. Setup visual controls and layout
    setupUIComponents();
    
    // 3. Register as a listener to processor streaming events
    audioProcessor.addListener (this);
    
    // 4. Load persisted settings and update visual state
    loadSettingsFromProcessor();
    updatePresetsCombo();
    updateUIState();
    
    // 5. Start a periodic UI status timer
    startTimer (250);
}

DawCastAudioProcessorEditor::~DawCastAudioProcessorEditor()
{
    audioProcessor.removeListener (this);
}

void DawCastAudioProcessorEditor::setupUIComponents()
{
    // Custom setup lambda for TextEditors to enforce style and accessibility fields
    auto setupTextEditor = [this](juce::TextEditor& editor, juce::Label& label, const juce::String& labelText,
                                  const juce::String& title, const juce::String& desc, int focusOrder, bool isPassword = false)
    {
        addAndMakeVisible (editor);
        editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x171C2E));
        editor.setColour (juce::TextEditor::textColourId, juce::Colour (0xFFE3E8F4));
        editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF2F3B5C));
        editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFF00E5FF));
        editor.setWantsKeyboardFocus (true);
        editor.setExplicitFocusOrder (focusOrder);
        
        if (isPassword)
            editor.setPasswordCharacter ('*');

        // Set screen reader info
        editor.setTitle (title);
        editor.setDescription (desc);

        // Attach accessibility label
        addAndMakeVisible (label);
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredRight);
        label.setColour (juce::Label::textColourId, juce::Colour (0xFF9CAAC6));
        label.attachToComponent (&editor, true);

        // Track when user manually modifies the text
        editor.onTextChange = [this]() { userModifiedSettings(); };
    };

    // Custom setup lambda for ComboBoxes
    auto setupComboBox = [this](juce::ComboBox& combo, juce::Label& label, const juce::String& labelText,
                                 const juce::String& title, const juce::String& desc, int focusOrder)
    {
        addAndMakeVisible (combo);
        combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0x171C2E));
        combo.setColour (juce::ComboBox::textColourId, juce::Colour (0xFFE3E8F4));
        combo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xFF2F3B5C));
        combo.setWantsKeyboardFocus (true);
        combo.setExplicitFocusOrder (focusOrder);

        // Set screen reader info
        combo.setTitle (title);
        combo.setDescription (desc);

        // Attach accessibility label
        addAndMakeVisible (label);
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredRight);
        label.setColour (juce::Label::textColourId, juce::Colour (0xFF9CAAC6));
        label.attachToComponent (&combo, true);

        // Track when user modifies settings dropdown
        combo.onChange = [this]() { userModifiedSettings(); };
    };

    // Title Header
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("DawCast Live Broadcaster", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00E5FF));
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    // Status Display - Focusable to allow screenreader checking (Shifted focus to 21)
    addAndMakeVisible (statusLabel);
    statusLabel.setWantsKeyboardFocus (true);
    statusLabel.setExplicitFocusOrder (21);
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setFont (juce::Font (14.0f, juce::Font::italic));
    statusLabel.setTitle ("Connection Status");
    statusLabel.setDescription ("Reports the current broadcast status to the Icecast server.");

    // Section Headers
    addAndMakeVisible (encoderHeader);
    encoderHeader.setText ("1. Audio & Encoder", juce::dontSendNotification);
    encoderHeader.setFont (juce::Font (16.0f, juce::Font::bold));
    encoderHeader.setColour (juce::Label::textColourId, juce::Colour (0xFFE3E8F4));

    addAndMakeVisible (serverHeader);
    serverHeader.setText ("2. Icecast Connection", juce::dontSendNotification);
    serverHeader.setFont (juce::Font (16.0f, juce::Font::bold));
    serverHeader.setColour (juce::Label::textColourId, juce::Colour (0xFFE3E8F4));

    addAndMakeVisible (streamHeader);
    streamHeader.setText ("3. Metadata & Details", juce::dontSendNotification);
    streamHeader.setFont (juce::Font (16.0f, juce::Font::bold));
    streamHeader.setColour (juce::Label::textColourId, juce::Colour (0xFFE3E8F4));

    // --- Presets UI Controls ---
    addAndMakeVisible (presetCombo);
    presetCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0x171C2E));
    presetCombo.setColour (juce::ComboBox::textColourId, juce::Colour (0xFFE3E8F4));
    presetCombo.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xFF2F3B5C));
    presetCombo.setWantsKeyboardFocus (true);
    presetCombo.setExplicitFocusOrder (1);
    presetCombo.setTitle ("Connection Presets");
    presetCombo.setDescription ("Select a saved streaming configuration preset.");
    
    addAndMakeVisible (presetLabel);
    presetLabel.setText ("Preset:", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    presetLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF9CAAC6));
    presetLabel.attachToComponent (&presetCombo, true);

    addAndMakeVisible (presetSaveButton);
    presetSaveButton.setButtonText ("Save");
    presetSaveButton.setWantsKeyboardFocus (true);
    presetSaveButton.setExplicitFocusOrder (2);
    presetSaveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x171C2E));
    presetSaveButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF00E5FF));
    presetSaveButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE3E8F4));
    presetSaveButton.setTitle ("Save Preset Button");
    presetSaveButton.setDescription ("Save current streaming configuration settings as a new preset.");
    presetSaveButton.onClick = [this]() { savePresetClicked(); };

    addAndMakeVisible (presetDeleteButton);
    presetDeleteButton.setButtonText ("Delete");
    presetDeleteButton.setWantsKeyboardFocus (true);
    presetDeleteButton.setExplicitFocusOrder (3);
    presetDeleteButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x171C2E));
    presetDeleteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF00E5FF));
    presetDeleteButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE3E8F4));
    presetDeleteButton.setTitle ("Delete Preset Button");
    presetDeleteButton.setDescription ("Delete the currently selected preset.");
    presetDeleteButton.onClick = [this]() { deletePresetClicked(); };

    presetCombo.onChange = [this]()
    {
        auto selectedPreset = presetCombo.getText();
        if (selectedPreset == "Custom" || selectedPreset.isEmpty())
            return;
            
        auto s = audioProcessor.getPresetSettings (selectedPreset);
        
        isUpdatingFromPreset = true;
        
        hostInput.setText (s.host, false);
        portInput.setText (juce::String (s.port), false);
        userInput.setText (s.username, false);
        passInput.setText (s.password, false);
        mountInput.setText (s.mountPoint, false);
        
        nameInput.setText (s.streamName, false);
        genreInput.setText (s.streamGenre, false);
        descInput.setText (s.streamDescription, false);
        urlInput.setText (s.streamUrl, false);
        
        artistInput.setText (s.currentArtist, false);
        titleInput.setText (s.currentTitle, false);
        
        formatCombo.setSelectedId (s.format == "ogg" ? 2 : 1, juce::dontSendNotification);
        
        int bitrateId = -1;
        for (int i = 1; i <= bitrateCombo.getNumItems(); ++i)
        {
            if (bitrateCombo.getItemText (i - 1).getIntValue() == s.bitrateKbps)
            {
                bitrateId = i;
                break;
            }
        }
        if (bitrateId != -1)
            bitrateCombo.setSelectedId (bitrateId, juce::dontSendNotification);
            
        if (s.targetSampleRate == 44100)
            sampleRateCombo.setSelectedId (2, juce::dontSendNotification);
        else if (s.targetSampleRate == 48000)
            sampleRateCombo.setSelectedId (3, juce::dontSendNotification);
        else
            sampleRateCombo.setSelectedId (1, juce::dontSendNotification);
            
        isUpdatingFromPreset = false;
    };

    // --- Instantiating controls with focus order and screenreader texts ---

    // Left Column Controls (Focus 4 - 6)
    setupComboBox (formatCombo, formatLabel, "Format:", 
                   "Stream Format", "Select between MP3 and OGG Vorbis streaming encoding", 4);
    formatCombo.addItem ("MP3 (LAME)", 1);
    formatCombo.addItem ("OGG (Vorbis)", 2);
    formatCombo.setSelectedId (1, juce::dontSendNotification);

    setupComboBox (bitrateCombo, bitrateLabel, "Bitrate:", 
                   "Bitrate", "Select encoding bitrate in kilobits per second", 5);
    bitrateCombo.addItem ("64 kbps", 1);
    bitrateCombo.addItem ("96 kbps", 2);
    bitrateCombo.addItem ("128 kbps", 3);
    bitrateCombo.addItem ("160 kbps", 4);
    bitrateCombo.addItem ("192 kbps", 5);
    bitrateCombo.addItem ("256 kbps", 6);
    bitrateCombo.addItem ("320 kbps", 7);
    bitrateCombo.setSelectedId (3, juce::dontSendNotification);

    setupComboBox (sampleRateCombo, sampleRateLabel, "Samplerate:", 
                   "Sample Rate", "Choose whether to match DAW sample rate or downsample to standard streaming rates", 6);
    sampleRateCombo.addItem ("DAW Native", 1);
    sampleRateCombo.addItem ("44.1 kHz", 2);
    sampleRateCombo.addItem ("48.0 kHz", 3);
    sampleRateCombo.setSelectedId (1, juce::dontSendNotification);

    // Middle Column Controls (Focus 7 - 11)
    setupTextEditor (hostInput, hostLabel, "Server IP:", 
                     "Server Hostname", "Enter the server's IP address or domain name", 7);
    
    setupTextEditor (portInput, portLabel, "Port:", 
                     "Port Number", "Enter the server port number (default is 8000)", 8);
    
    setupTextEditor (userInput, userLabel, "Username:", 
                     "Source Username", "Enter Icecast streaming username (default is source)", 9);
    
    setupTextEditor (passInput, passLabel, "Password:", 
                     "Source Password", "Enter Icecast mountpoint password", 10, true);
    
    setupTextEditor (mountInput, mountLabel, "Mount:", 
                     "Mount Point", "Enter target mountpoint (e.g. /live.mp3)", 11);

    // Right Column Controls (Focus 12 - 17)
    setupTextEditor (nameInput, nameLabel, "Name:", 
                     "Stream Name", "Enter the public name of your broadcast station", 12);
    
    setupTextEditor (genreInput, genreLabel, "Genre:", 
                     "Stream Genre", "Enter broadcast genre tags (e.g. Electronic, Rock)", 13);
                     
    setupTextEditor (descInput, descLabel, "Desc:", 
                     "Stream Description", "Enter stream description metadata", 14);
                     
    setupTextEditor (urlInput, urlLabel, "URL:", 
                     "Stream URL", "Enter stream homepage URL link", 15);
                     
    setupTextEditor (artistInput, artistLabel, "Artist:", 
                     "Current Artist", "Enter active artist metadata for stream overlays", 16);
                     
    setupTextEditor (titleInput, titleLabelField, "Title:", 
                     "Current Title", "Enter active song title metadata for stream overlays", 17);

    // Footer Buttons (Focus 18 - 19)
    addAndMakeVisible (connectButton);
    connectButton.setWantsKeyboardFocus (true);
    connectButton.setExplicitFocusOrder (19);
    connectButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x171C2E));
    connectButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF00E5FF));
    connectButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE3E8F4));
    connectButton.setTitle ("Connect Button");
    connectButton.setDescription ("Starts or stops broadcasting live DAW audio to the Icecast server.");
    connectButton.onClick = [this]()
    {
        if (audioProcessor.isCurrentlyStreaming())
        {
            audioProcessor.stopStreaming();
        }
        else if (!audioProcessor.isCurrentlyConnecting())
        {
            saveSettingsToProcessor();
            audioProcessor.startStreaming (audioProcessor.getConnectionSettings());
        }
    };

    addAndMakeVisible (updateMetadataButton);
    updateMetadataButton.setButtonText ("Update Track Info");
    updateMetadataButton.setWantsKeyboardFocus (true);
    updateMetadataButton.setExplicitFocusOrder (18);
    updateMetadataButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x171C2E));
    updateMetadataButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF00E5FF));
    updateMetadataButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE3E8F4));
    updateMetadataButton.setTitle ("Update Track Info Button");
    updateMetadataButton.setDescription ("Pushes the current Artist and Title to the Icecast server out-of-band.");
    updateMetadataButton.onClick = [this]()
    {
        saveSettingsToProcessor();
        audioProcessor.updateLiveMetadata (artistInput.getText(), titleInput.getText());
    };
}

void DawCastAudioProcessorEditor::resized()
{
    // Title / Status header
    titleLabel.setBounds (20, 15, 300, 30);
    presetCombo.setBounds (420, 15, 160, 30);
    presetSaveButton.setBounds (595, 15, 95, 30);
    presetDeleteButton.setBounds (700, 15, 100, 30);

    statusLabel.setBounds (20, 50, getWidth() - 40, 20);

    int colY = 80;
    int colH = 390;

    // Left Column: Audio Options (x = 20, width = 240)
    encoderHeader.setBounds (20, colY, 240, 25);
    formatCombo.setBounds (110, colY + 45, 135, 28);
    bitrateCombo.setBounds (110, colY + 95, 135, 28);
    sampleRateCombo.setBounds (110, colY + 145, 135, 28);

    // Middle Column: Server details (x = 280, width = 260)
    serverHeader.setBounds (280, colY, 260, 25);
    hostInput.setBounds (380, colY + 45, 145, 28);
    portInput.setBounds (380, colY + 95, 145, 28);
    userInput.setBounds (380, colY + 145, 145, 28);
    passInput.setBounds (380, colY + 195, 145, 28);
    mountInput.setBounds (380, colY + 245, 145, 28);

    // Right Column: Stream Info & Metadata (x = 560, width = 240)
    streamHeader.setBounds (560, colY, 240, 25);
    nameInput.setBounds (655, colY + 45, 135, 28);
    genreInput.setBounds (655, colY + 90, 135, 28);
    descInput.setBounds (655, colY + 135, 135, 28);
    urlInput.setBounds (655, colY + 180, 135, 28);
    artistInput.setBounds (655, colY + 235, 135, 28);
    titleInput.setBounds (655, colY + 280, 135, 28);

    // Footer buttons layout
    int btnY = 495;
    int btnH = 45;
    
    // Symmetrical layout
    connectButton.setBounds (280, btnY, 260, btnH);
    updateMetadataButton.setBounds (560, btnY, 230, btnH);
}

void DawCastAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Draw rich space-dark background
    g.fillAll (juce::Colour (0xFF0E111F));

    // Radial gradient glow behind components
    juce::ColourGradient gradient (juce::Colour (0x1F00E5FF), static_cast<float>(getWidth()) * 0.5f, static_cast<float>(getHeight()) * 0.5f,
                                   juce::Colour (0x000E111F), 0.0f, static_cast<float>(getWidth()) * 0.7f, true);
    g.setGradientFill (gradient);
    g.fillAll();

    // Graphic card drawer
    auto drawGlassPanel = [&g](juce::Rectangle<int> area, const juce::String& title)
    {
        g.setColour (juce::Colour (0x0EFFFFFF));
        g.fillRoundedRectangle (area.toFloat(), 10.0f);
        
        g.setColour (juce::Colour (0x15FFFFFF));
        g.drawRoundedRectangle (area.toFloat(), 10.0f, 1.2f);

        // Subheader line separator below section titles
        g.setColour (juce::Colour (0x1A00E5FF));
        g.drawHorizontalLine (area.getY() + 32, static_cast<float>(area.getX() + 10), static_cast<float>(area.getRight() - 10));
    };

    // Draw three panels
    drawGlassPanel (juce::Rectangle<int> (15, 75, 245, 395), "Encoder Options");
    drawGlassPanel (juce::Rectangle<int> (275, 75, 270, 395), "Connection Details");
    drawGlassPanel (juce::Rectangle<int> (555, 75, 250, 395), "Stream Details");
}

void DawCastAudioProcessorEditor::connectionStateChanged (bool connected, bool connecting)
{
    juce::ignoreUnused (connected, connecting);
    updateUIState();
}

void DawCastAudioProcessorEditor::connectionFailed (const juce::String& errorMessage)
{
    // Show alert popup when connection fails
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                            "Connection Error",
                                            errorMessage,
                                            "OK");
}

void DawCastAudioProcessorEditor::updateUIState()
{
    bool streaming = audioProcessor.isCurrentlyStreaming();
    bool connecting = audioProcessor.isCurrentlyConnecting();

    // Disable configurations during streaming/connecting
    bool enableInputs = !streaming && !connecting;
    
    formatCombo.setEnabled (enableInputs);
    bitrateCombo.setEnabled (enableInputs);
    sampleRateCombo.setEnabled (enableInputs);
    
    hostInput.setEnabled (enableInputs);
    portInput.setEnabled (enableInputs);
    userInput.setEnabled (enableInputs);
    passInput.setEnabled (enableInputs);
    mountInput.setEnabled (enableInputs);
    
    nameInput.setEnabled (enableInputs);
    genreInput.setEnabled (enableInputs);
    descInput.setEnabled (enableInputs);
    urlInput.setEnabled (enableInputs);

    // Keep metadata updates always editable so stream title overlays can be updated live!
    artistInput.setEnabled (true);
    titleInput.setEnabled (true);
    updateMetadataButton.setEnabled (streaming);

    // Update buttons texts and styling
    if (streaming)
    {
        connectButton.setButtonText ("Disconnect");
        connectButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF721C1C)); // Dark red
        
        juce::String status;
        status << "Broadcasting (" << formatCombo.getText() << " @ " << bitrateCombo.getText() << ")";
        statusLabel.setText (status, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF39FF14)); // Neon green
        statusLabel.setDescription ("Broadcast status is currently connected. " + status);
    }
    else if (connecting)
    {
        connectButton.setButtonText ("Connecting...");
        connectButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x6000E5FF)); // Dull Cyan
        
        statusLabel.setText ("Establishing connection...", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFE600)); // Glowing Yellow
        statusLabel.setDescription ("Broadcast status is currently connecting to the server.");
    }
    else
    {
        connectButton.setButtonText ("Connect");
        connectButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x1C2337)); // Slate Blue
        
        statusLabel.setText ("Status: Disconnected", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF9CAAC6)); // Dull grey
        statusLabel.setDescription ("Broadcast status is currently disconnected.");
    }
}

void DawCastAudioProcessorEditor::timerCallback()
{
    // Auto-update button state in case processor disconnected in the background
    bool streaming = audioProcessor.isCurrentlyStreaming();
    bool connecting = audioProcessor.isCurrentlyConnecting();

    if (!connecting)
    {
        if (streaming && connectButton.getButtonText() == "Connect")
            updateUIState();
        else if (!streaming && connectButton.getButtonText() == "Disconnect")
            updateUIState();
    }
}

void DawCastAudioProcessorEditor::loadSettingsFromProcessor()
{
    const auto& s = audioProcessor.getConnectionSettings();
    hostInput.setText (s.host, false);
    portInput.setText (juce::String (s.port), false);
    userInput.setText (s.username, false);
    passInput.setText (s.password, false);
    mountInput.setText (s.mountPoint, false);
    
    nameInput.setText (s.streamName, false);
    genreInput.setText (s.streamGenre, false);
    descInput.setText (s.streamDescription, false);
    urlInput.setText (s.streamUrl, false);
    
    artistInput.setText (s.currentArtist, false);
    titleInput.setText (s.currentTitle, false);
    
    formatCombo.setSelectedId (s.format == "ogg" ? 2 : 1, juce::dontSendNotification);
    
    // Find bitrate in combobox
    int bitrateId = -1;
    for (int i = 1; i <= bitrateCombo.getNumItems(); ++i)
    {
        if (bitrateCombo.getItemText (i - 1).getIntValue() == s.bitrateKbps)
        {
            bitrateId = i;
            break;
        }
    }
    if (bitrateId != -1)
        bitrateCombo.setSelectedId (bitrateId, juce::dontSendNotification);
    else
        bitrateCombo.setSelectedId (3, juce::dontSendNotification); // Default 128 kbps
        
    // Find target sample rate in combobox
    if (s.targetSampleRate == 44100)
        sampleRateCombo.setSelectedId (2, juce::dontSendNotification);
    else if (s.targetSampleRate == 48000)
        sampleRateCombo.setSelectedId (3, juce::dontSendNotification);
    else
        sampleRateCombo.setSelectedId (1, juce::dontSendNotification); // Default same as DAW
}

void DawCastAudioProcessorEditor::saveSettingsToProcessor()
{
    DawCastAudioProcessor::ConnectionSettings s;
    s.host = hostInput.getText();
    s.port = portInput.getText().getIntValue();
    s.username = userInput.getText();
    s.password = passInput.getText();
    s.mountPoint = mountInput.getText();
    
    s.format = formatCombo.getSelectedId() == 2 ? "ogg" : "mp3";
    s.bitrateKbps = bitrateCombo.getText().getIntValue();
    
    int srId = sampleRateCombo.getSelectedId();
    if (srId == 2)
        s.targetSampleRate = 44100;
    else if (srId == 3)
        s.targetSampleRate = 48000;
    else
        s.targetSampleRate = 0; // Native DAW samplerate
        
    s.streamName = nameInput.getText();
    s.streamGenre = genreInput.getText();
    s.streamDescription = descInput.getText();
    s.streamUrl = urlInput.getText();
    
    s.currentArtist = artistInput.getText();
    s.currentTitle = titleInput.getText();
    
    // Push settings into processor
    audioProcessor.setConnectionSettings (s);
}

void DawCastAudioProcessorEditor::updatePresetsCombo()
{
    presetCombo.clear (juce::dontSendNotification);
    presetCombo.addItem ("Custom", 1);
    
    auto names = audioProcessor.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
    {
        presetCombo.addItem (names[i], i + 2);
    }
    
    presetCombo.setSelectedId (1, juce::dontSendNotification);
}

void DawCastAudioProcessorEditor::savePresetClicked()
{
    // Prompt the user for a name using an AlertWindow
    auto* alert = new juce::AlertWindow ("Save Preset",
                                         "Enter a name for the new preset:",
                                         juce::AlertWindow::QuestionIcon);
                                         
    alert->addTextEditor ("presetName", "", "Preset Name:");
    
    alert->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey, 0, 0));
    alert->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey, 0, 0));
    
    alert->enterModalState (true, juce::ModalCallbackFunction::create ([this, alert] (int result)
    {
        if (result == 1)
        {
            auto name = alert->getTextEditorContents ("presetName").trim();
            if (name.isNotEmpty())
            {
                DawCastAudioProcessor::ConnectionSettings s;
                s.host = hostInput.getText();
                s.port = portInput.getText().getIntValue();
                s.username = userInput.getText();
                s.password = passInput.getText();
                s.mountPoint = mountInput.getText();
                
                s.format = formatCombo.getSelectedId() == 2 ? "ogg" : "mp3";
                s.bitrateKbps = bitrateCombo.getText().getIntValue();
                
                int srId = sampleRateCombo.getSelectedId();
                s.targetSampleRate = (srId == 2) ? 44100 : ((srId == 3) ? 48000 : 0);
                
                s.streamName = nameInput.getText();
                s.streamGenre = genreInput.getText();
                s.streamDescription = descInput.getText();
                s.streamUrl = urlInput.getText();
                
                s.currentArtist = artistInput.getText();
                s.currentTitle = titleInput.getText();

                // Save to processor
                audioProcessor.savePreset (name, s);
                
                // Refresh presets combo
                updatePresetsCombo();
                
                // Select the newly created preset
                presetCombo.setText (name, juce::dontSendNotification);
            }
        }
        delete alert;
    }));
}

void DawCastAudioProcessorEditor::deletePresetClicked()
{
    auto selectedPreset = presetCombo.getText();
    if (selectedPreset.isEmpty() || selectedPreset == "Custom")
        return;

    juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
                                        "Delete Preset",
                                        "Are you sure you want to delete the preset \"" + selectedPreset + "\"?",
                                        "Yes", "No",
                                        nullptr,
                                        juce::ModalCallbackFunction::create ([this, selectedPreset] (int result)
                                        {
                                            if (result != 0) // Yes clicked
                                            {
                                                audioProcessor.deletePreset (selectedPreset);
                                                updatePresetsCombo();
                                                loadSettingsFromProcessor();
                                            }
                                        }));
}

void DawCastAudioProcessorEditor::userModifiedSettings()
{
    if (isUpdatingFromPreset)
        return;
        
    if (presetCombo.getText() != "Custom")
        presetCombo.setText ("Custom", juce::dontSendNotification);

    saveSettingsToProcessor();
}
