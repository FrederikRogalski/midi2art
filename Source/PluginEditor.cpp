/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AdalightSender.h"

//==============================================================================
Midi2ArtAudioProcessorEditor::Midi2ArtAudioProcessorEditor (Midi2ArtAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Install custom LookAndFeel (minimal, only tweaks knob / button drawing)
    customLookAndFeel = std::make_unique<Midi2ArtLookAndFeel>();
    setLookAndFeel (customLookAndFeel.get());

    // Set plugin window size
    setSize (600, 770);
    
    // Load background image if available
    loadBackgroundImage();
    
    // Title is now part of the background image, so we don't need to display it
    // titleLabel is kept in the class but not made visible
    
    // ============================================================================
    // COLOR SECTION (Most Prominent - Top)
    // ============================================================================
    
    // Hidden sliders for color (bound to parameters)
    hueSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    hueSlider.setRange(0.0, 1.0, 0.001);
    satSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    satSlider.setRange(0.0, 1.0, 0.001);
    valSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    valSlider.setRange(0.0, 1.0, 0.001);
    
    // Color parameter attachments
    hueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_COLOR_HUE, hueSlider);
    satAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_COLOR_SAT, satSlider);
    valAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_COLOR_VAL, valSlider);
    
    // Color Selector - using JUCE's built-in component
    // Initialize with current parameter values
    float h = static_cast<float>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_COLOR_HUE));
    float s = static_cast<float>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_COLOR_SAT));
    float v = static_cast<float>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_COLOR_VAL));
    colourSelector.setCurrentColour(juce::Colour::fromHSV(h, s, v, 1.0f));
    
    colourSelector.setColour(juce::ColourSelector::backgroundColourId, juce::Colour(0x00000000));
    colourSelector.setSize(200, 200);
    colourSelector.addChangeListener(this);
    
    // Sync color selector when sliders change
    hueSlider.onValueChange = [this] {
        float h = static_cast<float>(hueSlider.getValue());
        float s = static_cast<float>(satSlider.getValue());
        float v = static_cast<float>(valSlider.getValue());
        colourSelector.setCurrentColour(juce::Colour::fromHSV(h, s, v, 1.0f));
    };
    satSlider.onValueChange = [this] {
        float h = static_cast<float>(hueSlider.getValue());
        float s = static_cast<float>(satSlider.getValue());
        float v = static_cast<float>(valSlider.getValue());
        colourSelector.setCurrentColour(juce::Colour::fromHSV(h, s, v, 1.0f));
    };
    valSlider.onValueChange = [this] {
        float h = static_cast<float>(hueSlider.getValue());
        float s = static_cast<float>(satSlider.getValue());
        float v = static_cast<float>(valSlider.getValue());
        colourSelector.setCurrentColour(juce::Colour::fromHSV(h, s, v, 1.0f));
    };
    
    addAndMakeVisible(colourSelector);
    
    // ============================================================================
    // ADSR SECTION (Prominent - Second)
    // ============================================================================
    
    // Attack
    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centred);
    attackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(attackLabel);
    
    attackValueLabel.setText("0.01s", juce::dontSendNotification);
    attackValueLabel.setJustificationType(juce::Justification::centred);
    attackValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(attackValueLabel);
    
    attackSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    attackSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    attackSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
    attackSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff808080));
    addAndMakeVisible(attackSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_ATTACK, attackSlider);
    attackSlider.onValueChange = [this] { updateKnobValueLabels(); };
    
    // Decay
    decayLabel.setText("Decay", juce::dontSendNotification);
    decayLabel.setJustificationType(juce::Justification::centred);
    decayLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(decayLabel);
    
    decayValueLabel.setText("0.1s", juce::dontSendNotification);
    decayValueLabel.setJustificationType(juce::Justification::centred);
    decayValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(decayValueLabel);
    
    decaySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    decaySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    decaySlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
    decaySlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff808080));
    addAndMakeVisible(decaySlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_DECAY, decaySlider);
    decaySlider.onValueChange = [this] { updateKnobValueLabels(); };
    
    // Sustain
    sustainLabel.setText("Sustain", juce::dontSendNotification);
    sustainLabel.setJustificationType(juce::Justification::centred);
    sustainLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(sustainLabel);
    
    sustainValueLabel.setText("0.7", juce::dontSendNotification);
    sustainValueLabel.setJustificationType(juce::Justification::centred);
    sustainValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(sustainValueLabel);
    
    sustainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sustainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sustainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
    sustainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff808080));
    addAndMakeVisible(sustainSlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_SUSTAIN, sustainSlider);
    sustainSlider.onValueChange = [this] { updateKnobValueLabels(); };
    
    // Release
    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centred);
    releaseLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(releaseLabel);
    
    releaseValueLabel.setText("0.2s", juce::dontSendNotification);
    releaseValueLabel.setJustificationType(juce::Justification::centred);
    releaseValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(releaseValueLabel);
    
    releaseSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    releaseSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
    releaseSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff808080));
    addAndMakeVisible(releaseSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_RELEASE, releaseSlider);
    releaseSlider.onValueChange = [this] { updateKnobValueLabels(); };
    
    // ============================================================================
    // LED CONFIGURATION SECTION (Setup - Third)
    // ============================================================================
    
    // LED Count
    ledCountLabel.setText("LED Count", juce::dontSendNotification);
    ledCountLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(ledCountLabel);
    ledCountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ledCountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    ledCountSlider.setInterceptsMouseClicks(true, true);
    addAndMakeVisible(ledCountSlider);
    ledCountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_LED_COUNT, ledCountSlider);
    ledCountSlider.onValueChange = [this] {
        updateLEDCountWarning();
    };
    
    // LED Offset
    ledOffsetLabel.setText("LED Offset", juce::dontSendNotification);
    ledOffsetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(ledOffsetLabel);
    ledOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ledOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    ledOffsetSlider.setInterceptsMouseClicks(true, true);
    addAndMakeVisible(ledOffsetSlider);
    ledOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_LED_OFFSET, ledOffsetSlider);
    ledOffsetSlider.onValueChange = [this] {
        updateLEDCountWarning();
    };
    
    // Lowest Note
    lowestNoteLabel.setText("Lowest Note", juce::dontSendNotification);
    lowestNoteLabel.setJustificationType(juce::Justification::centred);
    lowestNoteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(lowestNoteLabel);
    
    lowestNoteValueLabel.setText("C0", juce::dontSendNotification);
    lowestNoteValueLabel.setJustificationType(juce::Justification::centred);
    lowestNoteValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(lowestNoteValueLabel);
    
    lowestNoteSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    lowestNoteSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    lowestNoteSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe24a4a));
    lowestNoteSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff808080));
    lowestNoteSlider.textFromValueFunction = [](double value) {
        int note = static_cast<int>(value);
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int octave = (note / 12) - 1;
        int noteIndex = note % 12;
        return juce::String(noteNames[noteIndex]) + juce::String(octave);
    };
    addAndMakeVisible(lowestNoteSlider);
    lowestNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_LOWEST_NOTE, lowestNoteSlider);
    lowestNoteSlider.onValueChange = [this] { updateKnobValueLabels(); };
    
    lowestNoteLearnButton.setButtonText("Learn");
    lowestNoteLearnButton.setClickingTogglesState(true);
    lowestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
    lowestNoteLearnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    lowestNoteLearnButton.onClick = [this] {
        auto state = audioProcessor.getMidiLearnState();
        if (state == Midi2ArtAudioProcessor::MidiLearnState::LearningLowestNote)
        {
            audioProcessor.setMidiLearnState(Midi2ArtAudioProcessor::MidiLearnState::None);
            lowestNoteLearnButton.setToggleState(false, juce::dontSendNotification);
            lowestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
        }
        else
        {
            audioProcessor.setMidiLearnState(Midi2ArtAudioProcessor::MidiLearnState::LearningLowestNote);
            highestNoteLearnButton.setToggleState(false, juce::dontSendNotification);
            highestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
            lowestNoteLearnButton.setToggleState(true, juce::dontSendNotification);
            lowestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a90e2));
        }
        repaint();
    };
    addAndMakeVisible(lowestNoteLearnButton);
    
    // Highest Note
    highestNoteLabel.setText("Highest Note", juce::dontSendNotification);
    highestNoteLabel.setJustificationType(juce::Justification::centred);
    highestNoteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(highestNoteLabel);
    
    highestNoteValueLabel.setText("G8", juce::dontSendNotification);
    highestNoteValueLabel.setJustificationType(juce::Justification::centred);
    highestNoteValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(highestNoteValueLabel);
    
    highestNoteSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    highestNoteSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    highestNoteSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffe24a4a));
    highestNoteSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff808080));
    highestNoteSlider.textFromValueFunction = [](double value) {
        int note = static_cast<int>(value);
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int octave = (note / 12) - 1;
        int noteIndex = note % 12;
        return juce::String(noteNames[noteIndex]) + juce::String(octave);
    };
    addAndMakeVisible(highestNoteSlider);
    highestNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), Midi2ArtAudioProcessor::PARAM_HIGHEST_NOTE, highestNoteSlider);
    highestNoteSlider.onValueChange = [this] { updateKnobValueLabels(); };
    
    highestNoteLearnButton.setButtonText("Learn");
    highestNoteLearnButton.setClickingTogglesState(true);
    highestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
    highestNoteLearnButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    highestNoteLearnButton.onClick = [this] {
        auto state = audioProcessor.getMidiLearnState();
        if (state == Midi2ArtAudioProcessor::MidiLearnState::LearningHighestNote)
        {
            audioProcessor.setMidiLearnState(Midi2ArtAudioProcessor::MidiLearnState::None);
            highestNoteLearnButton.setToggleState(false, juce::dontSendNotification);
            highestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
        }
        else
        {
            audioProcessor.setMidiLearnState(Midi2ArtAudioProcessor::MidiLearnState::LearningHighestNote);
            lowestNoteLearnButton.setToggleState(false, juce::dontSendNotification);
            lowestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
            highestNoteLearnButton.setToggleState(true, juce::dontSendNotification);
            highestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a90e2));
        }
        repaint();
    };
    addAndMakeVisible(highestNoteLearnButton);
    
    // ============================================================================
    // NETWORK SECTION (Bottom - Smallest)
    // ============================================================================
    
    // Protocol Selection
    protocolLabel.setText("Protocol", juce::dontSendNotification);
    protocolLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(protocolLabel);
    
    protocolComboBox.addItem("Art-Net", 1);         // ID 1 = protocol 0
    protocolComboBox.addItem("E1.31 (sACN)", 2);    // ID 2 = protocol 1
    protocolComboBox.addItem("Adalight (USB)", 3);  // ID 3 = protocol 2
    protocolComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    protocolComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    protocolComboBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    protocolComboBox.onChange = [this] {
        // Map ComboBox ID (1,2,3) to parameter value (0,1,2)
        int selectedId = protocolComboBox.getSelectedId();
        int protocolValue = selectedId - 1; // 1->0 (Art-Net), 2->1 (E1.31), 3->2 (Adalight)
        audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_PROTOCOL)
            ->setValueNotifyingHost(audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_PROTOCOL)
                ->convertTo0to1(protocolValue));
        updateConnectionUI();
    };
    // Set initial value from parameter
    int currentProtocol = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_PROTOCOL));
    protocolComboBox.setSelectedId(currentProtocol + 1); // 0->1, 1->2, 2->3
    addAndMakeVisible(protocolComboBox);
    
    // Target IP Address / Serial Port (context-aware)
    connectionTargetLabel.setText("Target IP", juce::dontSendNotification);
    connectionTargetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(connectionTargetLabel);
    
    // IP Address Editor (for network protocols)
    ipAddressEditor.setMultiLine(false);
    ipAddressEditor.setReturnKeyStartsNewLine(false);
    ipAddressEditor.setText(audioProcessor.getValueTreeState().state.getProperty(Midi2ArtAudioProcessor::PARAM_WLED_IP, "239.255.0.1").toString(), juce::dontSendNotification);
    ipAddressEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    ipAddressEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    ipAddressEditor.setBorder(juce::BorderSize<int>(0, 8, 0, 8));
    ipAddressEditor.onTextChange = [this] {
        audioProcessor.getValueTreeState().state.setProperty(Midi2ArtAudioProcessor::PARAM_WLED_IP, ipAddressEditor.getText(), nullptr);
    };
    ipAddressEditor.onReturnKey = [this] {
        ipAddressEditor.giveAwayKeyboardFocus();
    };
    addAndMakeVisible(ipAddressEditor);
    
    // Serial Port ComboBox (for Adalight)
    serialPortComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    serialPortComboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    serialPortComboBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    serialPortComboBox.onChange = [this] {
        int selectedId = serialPortComboBox.getSelectedId();
        if (selectedId > 0)
        {
            juce::String portName = serialPortComboBox.getItemText(selectedId - 1);
            
            // Ignore the "(disconnected)" placeholder entry
            if (portName.contains("(disconnected)"))
                return;
            
            // Remember the user's explicit choice for auto-reconnect
            lastUserSelectedSerialPort = portName;
            audioProcessor.getValueTreeState().state.setProperty(Midi2ArtAudioProcessor::PARAM_SERIAL_PORT, portName, nullptr);
            updateStatusLabel();
        }
    };
    addAndMakeVisible(serialPortComboBox);
    
    // Universe/Baud Rate selector
    universeLabel.setText("Universe", juce::dontSendNotification);
    universeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(universeLabel);
    
    // Universe editor (for Art-Net/E1.31)
    universeEditor.setMultiLine(false);
    universeEditor.setReturnKeyStartsNewLine(false);
    // Set input filter to only allow numeric input
    universeEditor.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(10, "0123456789"), true);
    // Set initial value from parameter
    int currentUniverse = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_UNIVERSE));
    universeEditor.setText(juce::String(currentUniverse), juce::dontSendNotification);
    universeEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    universeEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    universeEditor.setBorder(juce::BorderSize<int>(0, 8, 0, 8));
    universeEditor.onTextChange = [this] {
        juce::String text = universeEditor.getText();
        // Only update if text is not empty and is a valid number
        if (text.isNotEmpty())
        {
            int universeValue = text.getIntValue();
            universeValue = juce::jlimit(0, 63999, universeValue); // Clamp to valid range
            audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_UNIVERSE)
                ->setValueNotifyingHost(audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_UNIVERSE)
                    ->convertTo0to1(universeValue));
        }
    };
    universeEditor.onReturnKey = [this] {
        universeEditor.giveAwayKeyboardFocus();
    };
    addAndMakeVisible(universeEditor);
    
    // Baud Rate ComboBox (for Adalight)
    baudRateComboBox.addItem("57600", 1);
    baudRateComboBox.addItem("115200", 2);
    baudRateComboBox.addItem("230400", 3);
    baudRateComboBox.addItem("460800", 4);
    baudRateComboBox.addItem("921600", 5);
    baudRateComboBox.setSelectedId(2); // Default 115200
    baudRateComboBox.onChange = [this] {
        int selectedBaudRate = 115200; // default
        switch (baudRateComboBox.getSelectedId())
        {
            case 1: selectedBaudRate = 57600; break;
            case 2: selectedBaudRate = 115200; break;
            case 3: selectedBaudRate = 230400; break;
            case 4: selectedBaudRate = 460800; break;
            case 5: selectedBaudRate = 921600; break;
        }
        audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_BAUD_RATE)
            ->setValueNotifyingHost(audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_BAUD_RATE)
                ->convertTo0to1(selectedBaudRate));
        updateLEDCountWarning(); // Recalculate warning when baud rate changes
    };
    addAndMakeVisible(baudRateComboBox);
    
    // LED Count Warning Label
    ledCountWarningLabel.setText("", juce::dontSendNotification);
    ledCountWarningLabel.setJustificationType(juce::Justification::centredLeft);
    ledCountWarningLabel.setColour(juce::Label::textColourId, juce::Colour(255, 165, 0)); // Orange
    ledCountWarningLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(ledCountWarningLabel);
    
    // Status
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    addAndMakeVisible(statusLabel);
    
    // Initial updates
    updateKnobValueLabels();
    
    // Restore last user-selected serial port from saved state (for auto-reconnect)
    lastUserSelectedSerialPort = audioProcessor.getValueTreeState().state.getProperty(
        Midi2ArtAudioProcessor::PARAM_SERIAL_PORT, "").toString();
    
    updateStatusLabel();
    updateConnectionUI();
    
    // Listen to processor changes (active notes count, MIDI learn state)
    audioProcessor.addChangeListener(this);
}

Midi2ArtAudioProcessorEditor::~Midi2ArtAudioProcessorEditor()
{
    stopTimer();
    
    // Important: reset LookAndFeel before destruction
    setLookAndFeel (nullptr);

    audioProcessor.removeChangeListener(this);
}

void Midi2ArtAudioProcessorEditor::timerCallback()
{
    // Periodically check if the selected serial port is still available
    // This detects USB device disconnection
    refreshSerialPorts();
    updateStatusLabel();
}

//==============================================================================
void Midi2ArtAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Draw background image if available
    if (hasBackgroundImage && backgroundImage.isValid())
    {
        g.drawImage(backgroundImage, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
        // Kein Overlay mehr - Hintergrundbild soll vollständig deckend sein
    }
    else
    {
        // Default dark background
        g.fillAll(juce::Colour(0xff1a1a1a));
    }
    
    // Section backgrounds removed - background image provides visual grouping
}

void Midi2ArtAudioProcessorEditor::resized()
{
    const int margin = 20;
    const int padding = 15;
    
    // Calculate section bounds
    // More space at top for logo/background image
    colorSectionBounds = juce::Rectangle<int>(margin, 80, getWidth() - 2 * margin, 240);
    adsrSectionBounds = juce::Rectangle<int>(margin, 340, getWidth() - 2 * margin, 140);
    ledConfigSectionBounds = juce::Rectangle<int>(margin, 505, getWidth() - 2 * margin, 200);  // Etwas tiefer (5px)
    networkSectionBounds = juce::Rectangle<int>(margin, 710, getWidth() - 2 * margin, 50);  // Etwas höher (10px)
    
    // Update window height to accommodate status label below network section
    setSize(getWidth(), networkSectionBounds.getBottom() + 40);
    
    // ============================================================================
    // COLOR SECTION (Most Prominent)
    // ============================================================================
    juce::Rectangle<int> colorArea = colorSectionBounds.reduced(padding);
    colourSelector.setBounds(colorArea.getCentreX() - 100, colorArea.getY() + 10, 200, 200);
    
    // ============================================================================
    // ADSR SECTION (Prominent - Horizontal Row using FlexBox)
    // ============================================================================
    juce::Rectangle<int> adsrArea = adsrSectionBounds.reduced(padding);
    const int knobSize = 80;
    int y = adsrArea.getY() + 20;
    
    // Use FlexBox for even distribution (like CSS space-between)
    juce::FlexBox adsrFlexBox;
    adsrFlexBox.flexDirection = juce::FlexBox::Direction::row;
    adsrFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    adsrFlexBox.alignItems = juce::FlexBox::AlignItems::center;
    
    // Attack
    juce::FlexItem attackItem(attackSlider);
    attackItem.width = knobSize;
    attackItem.height = knobSize;
    adsrFlexBox.items.add(attackItem);
    
    // Decay
    juce::FlexItem decayItem(decaySlider);
    decayItem.width = knobSize;
    decayItem.height = knobSize;
    adsrFlexBox.items.add(decayItem);
    
    // Sustain
    juce::FlexItem sustainItem(sustainSlider);
    sustainItem.width = knobSize;
    sustainItem.height = knobSize;
    adsrFlexBox.items.add(sustainItem);
    
    // Release
    juce::FlexItem releaseItem(releaseSlider);
    releaseItem.width = knobSize;
    releaseItem.height = knobSize;
    adsrFlexBox.items.add(releaseItem);
    
    // Apply FlexBox layout
    juce::Rectangle<int> adsrKnobArea(adsrArea.getX(), y, adsrArea.getWidth(), knobSize);
    adsrFlexBox.performLayout(adsrKnobArea.toFloat());
    
    // Position labels relative to knobs
    attackValueLabel.setBounds(attackSlider.getX(), attackSlider.getY() - 18, knobSize, 18);
    attackLabel.setBounds(attackSlider.getX(), attackSlider.getY() + knobSize + 2, knobSize, 18);
    
    decayValueLabel.setBounds(decaySlider.getX(), decaySlider.getY() - 18, knobSize, 18);
    decayLabel.setBounds(decaySlider.getX(), decaySlider.getY() + knobSize + 2, knobSize, 18);
    
    sustainValueLabel.setBounds(sustainSlider.getX(), sustainSlider.getY() - 18, knobSize, 18);
    sustainLabel.setBounds(sustainSlider.getX(), sustainSlider.getY() + knobSize + 2, knobSize, 18);
    
    releaseValueLabel.setBounds(releaseSlider.getX(), releaseSlider.getY() - 18, knobSize, 18);
    releaseLabel.setBounds(releaseSlider.getX(), releaseSlider.getY() + knobSize + 2, knobSize, 18);
    
    // ============================================================================
    // LED CONFIGURATION SECTION
    // ============================================================================
    juce::Rectangle<int> ledArea = ledConfigSectionBounds.reduced(padding);
    const int sliderHeight = 30;
    const int sliderSpacing = 35;
    const int labelWidth = 100;
    const int labelHeight = 18;
    const int noteKnobSize = 70;
    const int learnButtonWidth = 60;
    const int learnButtonSpacing = 10;
    
    // Calculate total content height with warning
    const int warningHeight = 15;
    const int warningSpacing = 0;  // Spacing after warning
    const int sliderReducedSpacing = 22;  // Spacing between LED sliders
    int totalContentHeight = warningHeight + warningSpacing + 2 * sliderHeight + sliderReducedSpacing + noteKnobSize + labelHeight + 2;
    int availableHeight = ledArea.getHeight();
    int verticalPadding = (availableHeight - totalContentHeight) / 2;
    
    // Start from top with equal padding
    y = ledArea.getY() + verticalPadding;
    
    // LED Count Warning (above LED Offset)
    ledCountWarningLabel.setBounds(ledArea.getX(), y, ledArea.getWidth(), warningHeight);
    y += warningHeight + warningSpacing;
    
    // LED Offset (swapped - first)
    ledOffsetLabel.setBounds(ledArea.getX(), y, labelWidth - 10, sliderHeight);
    juce::Rectangle<int> offsetSliderBounds(ledArea.getX() + labelWidth, y, ledArea.getWidth() - labelWidth, sliderHeight);
    ledOffsetSlider.setBounds(offsetSliderBounds);
    y += sliderReducedSpacing;
    
    // LED Count (swapped - second)
    ledCountLabel.setBounds(ledArea.getX(), y, labelWidth - 10, sliderHeight);
    juce::Rectangle<int> countSliderBounds(ledArea.getX() + labelWidth, y, ledArea.getWidth() - labelWidth, sliderHeight);
    ledCountSlider.setBounds(countSliderBounds);
    y += sliderReducedSpacing + 20; // Compensate to keep note knobs in same position
    
    // Note range knobs - spaceBetween: leftmost and rightmost positions
    // Position Lowest Note (leftmost position)
    int lowestX = ledArea.getX();
    lowestNoteSlider.setBounds(lowestX, y, noteKnobSize, noteKnobSize);
    lowestNoteValueLabel.setBounds(lowestX, y - labelHeight, noteKnobSize, labelHeight);
    lowestNoteLabel.setBounds(lowestX, y + noteKnobSize + 2, noteKnobSize, labelHeight);
    lowestNoteLearnButton.setBounds(lowestX + noteKnobSize + learnButtonSpacing, y + noteKnobSize / 2 - 15, learnButtonWidth, 30);
    
    // Position Highest Note (rightmost position, accounting for Learn button)
    int highestX = ledArea.getRight() - noteKnobSize - learnButtonWidth - learnButtonSpacing;
    highestNoteSlider.setBounds(highestX, y, noteKnobSize, noteKnobSize);
    highestNoteValueLabel.setBounds(highestX, y - labelHeight, noteKnobSize, labelHeight);
    highestNoteLabel.setBounds(highestX, y + noteKnobSize + 2, noteKnobSize, labelHeight);
    highestNoteLearnButton.setBounds(highestX + noteKnobSize + learnButtonSpacing, y + noteKnobSize / 2 - 15, learnButtonWidth, 30);
    
    // ============================================================================
    // NETWORK SECTION (Bottom) - Horizontal Layout
    // ============================================================================
    juce::Rectangle<int> networkArea = networkSectionBounds.reduced(padding);
    int networkCenterY = networkArea.getCentreY();
    const int networkFieldHeight = 25;
    
    // Network row layout - structured for consistent spacing
    // Total width available in network section
    const int totalWidth = networkArea.getWidth();
    
    // Define consistent field widths
    const int networkLabelWidth = 70;           // All labels
    const int protocolComboWidth = 110;  // Protocol dropdown
    const int connectionFieldWidth = 120; // IP/Serial port field
    const int universeFieldWidth = 55;    // Universe number (network protocols)
    const int baudRateFieldWidth = 75;    // Baud rate selector (Adalight)
    
    // Calculate spacing: distribute remaining space evenly between the 3 field groups
    const int labelFieldPairs = 3;  // Protocol, Connection, Universe/BaudRate
    const int totalFieldsWidth = (networkLabelWidth + protocolComboWidth) + 
                                 (networkLabelWidth + connectionFieldWidth) + 
                                 (networkLabelWidth + baudRateFieldWidth);  // Use larger field width for spacing calculation
    const int remainingSpace = totalWidth - totalFieldsWidth;
    const int spacing = remainingSpace / (labelFieldPairs - 1);  // Space between field groups
    
    int x = networkArea.getX();
    
    // Protocol (Label + ComboBox)
    protocolLabel.setBounds(x, networkCenterY - networkFieldHeight / 2, networkLabelWidth, networkFieldHeight);
    protocolComboBox.setBounds(x + networkLabelWidth, networkCenterY - networkFieldHeight / 2, protocolComboWidth, networkFieldHeight);
    x += networkLabelWidth + protocolComboWidth + spacing;
    
    // Target IP / Serial Port (Label + Editor/ComboBox - context-aware)
    connectionTargetLabel.setBounds(x, networkCenterY - networkFieldHeight / 2, networkLabelWidth, networkFieldHeight);
    ipAddressEditor.setBounds(x + networkLabelWidth, networkCenterY - networkFieldHeight / 2, connectionFieldWidth, networkFieldHeight);
    serialPortComboBox.setBounds(x + networkLabelWidth, networkCenterY - networkFieldHeight / 2, connectionFieldWidth, networkFieldHeight);
    x += networkLabelWidth + connectionFieldWidth + spacing;
    
    // Universe/Baud Rate (Label + Editor/ComboBox)
    universeLabel.setBounds(x, networkCenterY - networkFieldHeight / 2, networkLabelWidth, networkFieldHeight);
    universeEditor.setBounds(x + networkLabelWidth, networkCenterY - networkFieldHeight / 2, universeFieldWidth, networkFieldHeight);
    baudRateComboBox.setBounds(x + networkLabelWidth, networkCenterY - networkFieldHeight / 2, baudRateFieldWidth, networkFieldHeight);
    
    // Status label below network section
    statusLabel.setBounds(margin, networkSectionBounds.getBottom() + 10, getWidth() - 2 * margin, 20);
}

void Midi2ArtAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // Click-outside: Wenn außerhalb der Text-Editors geklickt wird, Focus entfernen
    if (! ipAddressEditor.getBounds().contains (e.getPosition()) &&
        ! universeEditor.getBounds().contains (e.getPosition()))
    {
        ipAddressEditor.giveAwayKeyboardFocus();
        universeEditor.giveAwayKeyboardFocus();
    }
}

void Midi2ArtAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &colourSelector)
    {
        updateColorFromSelector();
    }
    else if (source == &audioProcessor)
    {
        // Update status label when active notes count changes
        updateStatusLabel();
        
        // Update MIDI learn button states when learn state changes
        auto learnState = audioProcessor.getMidiLearnState();
        if (learnState == Midi2ArtAudioProcessor::MidiLearnState::LearningLowestNote)
        {
            if (!lowestNoteLearnButton.getToggleState())
            {
                lowestNoteLearnButton.setToggleState(true, juce::dontSendNotification);
                lowestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a90e2));
                repaint();
            }
        }
        else if (learnState == Midi2ArtAudioProcessor::MidiLearnState::LearningHighestNote)
        {
            if (!highestNoteLearnButton.getToggleState())
            {
                highestNoteLearnButton.setToggleState(true, juce::dontSendNotification);
                highestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a90e2));
                repaint();
            }
        }
        else
        {
            if (lowestNoteLearnButton.getToggleState())
            {
                lowestNoteLearnButton.setToggleState(false, juce::dontSendNotification);
                lowestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
                repaint();
            }
            if (highestNoteLearnButton.getToggleState())
            {
                highestNoteLearnButton.setToggleState(false, juce::dontSendNotification);
                highestNoteLearnButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
                repaint();
            }
        }
    }
}

void Midi2ArtAudioProcessorEditor::updateColorFromSelector()
{
    juce::Colour currentColour = colourSelector.getCurrentColour();
    float h, s, v;
    currentColour.getHSB(h, s, v);
    
    // Update parameters directly (h, s, v are already 0.0-1.0, so use them directly)
    // setValueNotifyingHost expects normalized 0.0-1.0 values
    audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_COLOR_HUE)->setValueNotifyingHost(h);
    audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_COLOR_SAT)->setValueNotifyingHost(s);
    audioProcessor.getValueTreeState().getParameter(Midi2ArtAudioProcessor::PARAM_COLOR_VAL)->setValueNotifyingHost(v);
}

void Midi2ArtAudioProcessorEditor::updateKnobValueLabels()
{
    // ADSR values
    float attack = static_cast<float>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_ATTACK));
    attackValueLabel.setText(juce::String(attack, 2) + "s", juce::dontSendNotification);
    
    float decay = static_cast<float>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_DECAY));
    decayValueLabel.setText(juce::String(decay, 2) + "s", juce::dontSendNotification);
    
    float sustain = static_cast<float>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_SUSTAIN));
    sustainValueLabel.setText(juce::String(sustain, 2), juce::dontSendNotification);
    
    float release = static_cast<float>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_RELEASE));
    releaseValueLabel.setText(juce::String(release, 2) + "s", juce::dontSendNotification);
    
    // Note values
    int lowestNote = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_LOWEST_NOTE));
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (lowestNote / 12) - 1;
    int noteIndex = lowestNote % 12;
    lowestNoteValueLabel.setText(juce::String(noteNames[noteIndex]) + juce::String(octave), juce::dontSendNotification);
    
    int highestNote = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_HIGHEST_NOTE));
    octave = (highestNote / 12) - 1;
    noteIndex = highestNote % 12;
    highestNoteValueLabel.setText(juce::String(noteNames[noteIndex]) + juce::String(octave), juce::dontSendNotification);
}

void Midi2ArtAudioProcessorEditor::updateStatusLabel()
{
    int activeNotes = audioProcessor.getActiveNotesCount();
    
    // Priority 1: Show active notes if playing
    if (activeNotes > 0)
    {
        statusLabel.setText(juce::String(activeNotes) + (activeNotes == 1 ? " note active" : " notes active"), juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1fa0ff)); // Accent-Blau für aktive Notes
        return;
    }
    
    // Priority 2: Check serial port connection if Adalight is selected
    int currentProtocol = protocolComboBox.getSelectedId() - 1;
    if (currentProtocol == 2) // Adalight (USB)
    {
        checkSerialPortConnection();
        return;
    }
    
    // Priority 3: Default ready state
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
}

void Midi2ArtAudioProcessorEditor::checkSerialPortConnection()
{
    // Active port = currently connected and selected
    juce::String activePort = audioProcessor.getValueTreeState().state.getProperty(Midi2ArtAudioProcessor::PARAM_SERIAL_PORT, "").toString();
    
    if (activePort.isNotEmpty())
    {
        // We have an active connection
        juce::String shortName = activePort.fromLastOccurrenceOf("/", false, false);
        statusLabel.setText("Connected: " + shortName, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
        return;
    }
    
    // No active connection - are we waiting for a device to come back?
    if (lastUserSelectedSerialPort.isNotEmpty())
    {
        juce::String shortName = lastUserSelectedSerialPort.fromLastOccurrenceOf("/", false, false);
        statusLabel.setText("Disconnected: " + shortName + " - reconnecting...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        return;
    }
    
    // Nothing configured at all
    statusLabel.setText("No serial port selected", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
}

void Midi2ArtAudioProcessorEditor::loadBackgroundImage()
{
    // Load from BinaryData (image added as binary resource via Projucer)
    // Adjust name if your file is not exactly "Background.png"
    backgroundImage = juce::ImageCache::getFromMemory(
        BinaryData::Background_png,
        BinaryData::Background_pngSize);

    hasBackgroundImage = backgroundImage.isValid();
}

void Midi2ArtAudioProcessorEditor::updateConnectionUI()
{
    // Get protocol from ComboBox selection (more reliable than reading parameter)
    int selectedId = protocolComboBox.getSelectedId();
    int currentProtocol = selectedId - 1; // ComboBox IDs are 1-based
    bool isSerial = (currentProtocol == 2); // Adalight
    
    // Update label text
    connectionTargetLabel.setText(isSerial ? "Serial Port" : "Target IP", juce::dontSendNotification);
    
    // Show/hide appropriate control
    ipAddressEditor.setVisible(!isSerial);
    serialPortComboBox.setVisible(isSerial);
    
    // Update universe/baud rate section
    if (isSerial)
    {
        // Adalight: Show baud rate selector
        universeLabel.setText("Baud Rate", juce::dontSendNotification);
        universeEditor.setVisible(false);
        baudRateComboBox.setVisible(true);
        
        // Set current baud rate from parameter
        int currentBaudRate = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_BAUD_RATE));
        
        // Select appropriate baud rate in combobox
        if (currentBaudRate == 57600) baudRateComboBox.setSelectedId(1);
        else if (currentBaudRate == 115200) baudRateComboBox.setSelectedId(2);
        else if (currentBaudRate == 230400) baudRateComboBox.setSelectedId(3);
        else if (currentBaudRate == 460800) baudRateComboBox.setSelectedId(4);
        else if (currentBaudRate == 921600) baudRateComboBox.setSelectedId(5);
        else baudRateComboBox.setSelectedId(2); // default 115200
    }
    else
    {
        // Network protocols: Show universe number
        universeLabel.setText("Universe", juce::dontSendNotification);
        universeEditor.setVisible(true);
        baudRateComboBox.setVisible(false);
    }
    
    universeLabel.setVisible(true); // Always show the label, just change the text
    
    if (isSerial)
    {
        // Force full refresh when switching to serial - the port list may not
        // have changed, but the combobox state needs to be re-populated
        lastKnownSerialPorts.clear();
        refreshSerialPorts();
        
        // Start polling to detect USB disconnect (every 2 seconds)
        startTimer(2000);
    }
    else
    {
        // No need to poll when not using serial
        stopTimer();
    }
    
    // Update status to show connection state
    updateStatusLabel();
    
    // Update LED count warning
    updateLEDCountWarning();
}

void Midi2ArtAudioProcessorEditor::refreshSerialPorts()
{
    // Get available serial ports from AdalightSender
    juce::StringArray ports = AdalightSender::getAvailableSerialPorts();
    
    // Check if list changed - avoid unnecessary UI updates
    if (ports == lastKnownSerialPorts)
        return;
    
    lastKnownSerialPorts = ports;
    
    // The port the user originally chose (survives disconnect/reconnect cycles)
    juce::String desiredPort = lastUserSelectedSerialPort;
    
    // Clear and repopulate
    serialPortComboBox.clear(juce::dontSendNotification);
    
    if (desiredPort.isNotEmpty())
    {
        int portIndex = ports.indexOf(desiredPort);
        
        if (portIndex >= 0)
        {
            // Desired port is available - normal list, select it
            serialPortComboBox.setEnabled(true);
            for (int i = 0; i < ports.size(); i++)
                serialPortComboBox.addItem(ports[i], i + 1);
            
            serialPortComboBox.setSelectedId(portIndex + 1, juce::dontSendNotification);
            audioProcessor.getValueTreeState().state.setProperty(
                Midi2ArtAudioProcessor::PARAM_SERIAL_PORT, desiredPort, nullptr);
        }
        else
        {
            // Desired port disconnected - show it as first entry (greyed out hint),
            // then list available ports below for easy switching
            juce::String shortName = desiredPort.fromLastOccurrenceOf("/", false, false);
            serialPortComboBox.addItem(shortName + " (disconnected)", 1);
            
            for (int i = 0; i < ports.size(); i++)
                serialPortComboBox.addItem(ports[i], i + 2);
            
            serialPortComboBox.setSelectedId(1, juce::dontSendNotification);
            serialPortComboBox.setEnabled(true);
            audioProcessor.getValueTreeState().state.setProperty(
                Midi2ArtAudioProcessor::PARAM_SERIAL_PORT, "", nullptr);
        }
    }
    else if (ports.isEmpty())
    {
        serialPortComboBox.addItem("No serial ports found", 1);
        serialPortComboBox.setSelectedId(1, juce::dontSendNotification);
        serialPortComboBox.setEnabled(false);
    }
    else
    {
        // Ports available but nothing selected yet
        serialPortComboBox.setEnabled(true);
        for (int i = 0; i < ports.size(); i++)
            serialPortComboBox.addItem(ports[i], i + 1);
        serialPortComboBox.setSelectedId(0, juce::dontSendNotification);
    }
    
    // Update status after refresh
    updateStatusLabel();
}

int Midi2ArtAudioProcessorEditor::calculateMaxLEDCount(int baudRate, int fps)
{
    // Formula: max LEDs = ((baudRate / 10 / fps) - 6 header bytes) / 3 bytes per LED
    // 8N1 encoding: 10 bits per byte (1 start + 8 data + 1 stop)
    // Return exact value, rounded down
    int bytesPerSecond = baudRate / 10;
    int bytesPerFrame = bytesPerSecond / fps;
    int maxBytes = bytesPerFrame - 6; // Subtract 6-byte Adalight header
    int maxLEDs = maxBytes / 3; // Integer division automatically rounds down
    return juce::jmax(1, maxLEDs); // At least 1 LED
}

void Midi2ArtAudioProcessorEditor::updateLEDCountWarning()
{
    // Get protocol
    int selectedId = protocolComboBox.getSelectedId();
    int currentProtocol = selectedId - 1;
    bool isAdalight = (currentProtocol == 2);
    
    if (!isAdalight)
    {
        // No warning for network protocols
        ledCountWarningLabel.setText("", juce::dontSendNotification);
        return;
    }
    
    // Get current LED count, offset, and baud rate
    int ledCount = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_LED_COUNT));
    int ledOffset = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_LED_OFFSET));
    int baudRate = static_cast<int>(*audioProcessor.getValueTreeState().getRawParameterValue(Midi2ArtAudioProcessor::PARAM_BAUD_RATE));
    
    // Calculate max safe LED count
    int totalLEDs = ledOffset + ledCount;
    int maxSafeLEDs = calculateMaxLEDCount(baudRate, 30);
    
    if (totalLEDs > maxSafeLEDs)
    {
        juce::String warningText = "WARNING: LED Offset + Count = " + juce::String(totalLEDs) + 
                                   " exceeds recommended " + juce::String(maxSafeLEDs) + 
                                   " @ " + juce::String(baudRate) + " baud. May cause lag.";
        ledCountWarningLabel.setText(warningText, juce::dontSendNotification);
    }
    else
    {
        ledCountWarningLabel.setText("", juce::dontSendNotification);
    }
}
