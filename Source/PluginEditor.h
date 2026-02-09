/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Midi2ArtLookAndFeel.h"

//==============================================================================
/**
*/
class Midi2ArtAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       public juce::ChangeListener,
                                       public juce::Timer
{
public:
    Midi2ArtAudioProcessorEditor (Midi2ArtAudioProcessor&);
    ~Midi2ArtAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Midi2ArtAudioProcessor& audioProcessor;
    
    // UI Components - Color Section (Most Prominent)
    juce::ColourSelector colourSelector;
    
    // UI Components - ADSR Section (Prominent)
    juce::Slider attackSlider;
    juce::Label attackLabel;
    juce::Label attackValueLabel;
    
    juce::Slider decaySlider;
    juce::Label decayLabel;
    juce::Label decayValueLabel;
    
    juce::Slider sustainSlider;
    juce::Label sustainLabel;
    juce::Label sustainValueLabel;
    
    juce::Slider releaseSlider;
    juce::Label releaseLabel;
    juce::Label releaseValueLabel;
    
    // UI Components - LED Configuration Section
    juce::Slider ledCountSlider;
    juce::Label ledCountLabel;
    
    juce::Slider ledOffsetSlider;
    juce::Label ledOffsetLabel;
    
    juce::Slider lowestNoteSlider;
    juce::Label lowestNoteLabel;
    juce::Label lowestNoteValueLabel;
    juce::TextButton lowestNoteLearnButton;
    
    juce::Slider highestNoteSlider;
    juce::Label highestNoteLabel;
    juce::Label highestNoteValueLabel;
    juce::TextButton highestNoteLearnButton;
    
    // UI Components - Network Section
    juce::ComboBox protocolComboBox;
    juce::Label protocolLabel;
    
    juce::TextEditor ipAddressEditor;
    juce::ComboBox serialPortComboBox;
    juce::Label connectionTargetLabel;  // Dynamic label: "Target IP" or "Serial Port"
    
    juce::TextEditor universeEditor;
    juce::ComboBox baudRateComboBox;  // For Adalight protocol
    juce::Label universeLabel;  // Dynamic label: "Universe" or "Baud Rate"
    juce::Label ledCountWarningLabel;  // Warning when LED count exceeds safe limit
    
    juce::Label titleLabel;
    juce::Label statusLabel;
    
    // Background image support
    juce::Image backgroundImage;
    bool hasBackgroundImage = false;
    
    // Section bounds for layout calculations
    juce::Rectangle<int> colorSectionBounds;
    juce::Rectangle<int> adsrSectionBounds;
    juce::Rectangle<int> ledConfigSectionBounds;
    juce::Rectangle<int> networkSectionBounds;
    
    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ledCountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ledOffsetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowestNoteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highestNoteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    
    // Color parameter attachments (hidden sliders for parameter binding)
    juce::Slider hueSlider;
    juce::Slider satSlider;
    juce::Slider valSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hueAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> satAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valAttachment;

    // Custom LookAndFeel for knobs/buttons (kept very small & focused)
    std::unique_ptr<Midi2ArtLookAndFeel> customLookAndFeel;
    
    juce::StringArray lastKnownSerialPorts;
    juce::String lastUserSelectedSerialPort;  // Remembers user's choice for auto-reconnect
    
    // Change listener callback
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    // Timer callback for serial port disconnect detection
    void timerCallback() override;
    
    void updateColorFromSelector();
    void updateKnobValueLabels();
    void updateStatusLabel();
    void loadBackgroundImage();
    void updateConnectionUI();
    void refreshSerialPorts();
    void checkSerialPortConnection();
    void updateLEDCountWarning();
    int calculateMaxLEDCount(int baudRate, int fps = 30);  // Calculate max safe LED count for given baud rate

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Midi2ArtAudioProcessorEditor)
};

