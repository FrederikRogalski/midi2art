/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DMXSender.h"
#include "ArtNetSender.h"
#include "E131Sender.h"
#include "AdalightSender.h"

//==============================================================================
/**
*/
class Midi2ArtAudioProcessor  : public juce::AudioProcessor,
                                  public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    Midi2ArtAudioProcessor();
    ~Midi2ArtAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }
    
    // Get active notes count for UI display
    int getActiveNotesCount() const { return activeNotes.size(); }
    
    // Parameter IDs
    static constexpr const char* PARAM_LED_COUNT = "ledCount";
    static constexpr const char* PARAM_LED_OFFSET = "ledOffset";
    static constexpr const char* PARAM_LOWEST_NOTE = "lowestNote";
    static constexpr const char* PARAM_HIGHEST_NOTE = "highestNote";
    static constexpr const char* PARAM_ATTACK = "attack";
    static constexpr const char* PARAM_DECAY = "decay";
    static constexpr const char* PARAM_SUSTAIN = "sustain";
    static constexpr const char* PARAM_RELEASE = "release";
    static constexpr const char* PARAM_COLOR_HUE = "colorHue";
    static constexpr const char* PARAM_COLOR_SAT = "colorSat";
    static constexpr const char* PARAM_COLOR_VAL = "colorVal";
    static constexpr const char* PARAM_WLED_IP = "wledIP";
    static constexpr const char* PARAM_SERIAL_PORT = "serialPort";
    static constexpr const char* PARAM_PROTOCOL = "protocol";
    static constexpr const char* PARAM_UNIVERSE = "universe";  // Network protocols only (Art-Net, E1.31)
    static constexpr const char* PARAM_BAUD_RATE = "baudRate";  // Adalight serial only
    
    // MIDI learn state
    enum class MidiLearnState
    {
        None,
        LearningLowestNote,
        LearningHighestNote
    };
    
    MidiLearnState midiLearnState = MidiLearnState::None;
    void setMidiLearnState(MidiLearnState state) 
    { 
        if (midiLearnState != state)
        {
            midiLearnState = state; 
            sendChangeMessage(); // Notify UI that MIDI learn state changed
        }
    }
    MidiLearnState getMidiLearnState() const { return midiLearnState; }

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState parameters;
    
    // DMX protocol sender (abstract interface)
    std::unique_ptr<DMXSender> dmxSender;
    
    // Active notes tracking
    struct ActiveNote
    {
        int midiNote;
        int ledIndex;
        float velocity;
        ADSREnvelope envelope;
        juce::Colour color;
        float currentEnvelopeLevel = 0.0f; // Current envelope level for Art-Net output
        bool isSustained = false; // True if note is being held by sustain pedal
    };
    
    juce::Array<ActiveNote> activeNotes;
    
    // Sustain pedal state (CC 64)
    bool sustainPedalActive = false;
    
    // Current parameters
    int currentLEDCount = 88;
    int currentLEDOffset = 0;
    int currentLowestNote = 21;   // A0 (lowest key on 88-key piano)
    int currentHighestNote = 108; // C8 (highest key on 88-key piano)
    juce::String currentWLEDIP = "239.255.0.1";  // E1.31 Multicast Universe 1
    juce::String currentSerialPort = "";          // Serial port for Adalight
    int currentProtocol = 1;       // 0 = Art-Net, 1 = E1.31 (default), 2 = Adalight
    int currentUniverse = 1;       // Universe number for network protocols (Art-Net, E1.31)
    int currentBaudRate = 115200;  // Baud rate for Adalight serial
    juce::Colour currentColor = juce::Colours::white;
    
    // ADSR parameters (piano-like defaults)
    float attackTime = 0.1f;
    float decayTime = 0.7f;
    float sustainLevel = 0.1f;
    float releaseTime = 0.2f;
    
    // Sample rate for envelope calculation
    double sampleRate = 44100.0;
    
    // Update rate for LED output when notes are active (send at ~30Hz to avoid serial bandwidth saturation)
    // At 115200 baud: max 50.5 fps theoretical, 30 fps = 59% capacity (safe headroom)
    int updateCounter = 0;
    static constexpr int UPDATE_INTERVAL = 1470; // 44100 / 30 (samples between updates)
    
    // Previous LED count for visual feedback
    int previousLEDCount = 0;
    
    // Pre-allocated buffer for LED output (avoids heap allocation on audio thread)
    // Max size: 512 LEDs * 3 channels = 1536 bytes
    static constexpr int MAX_DMX_BUFFER_SIZE = 512 * 3;
    uint8_t dmxBuffer[MAX_DMX_BUFFER_SIZE] = {0};
    
    void updateParameters();
    void processMidiMessages(juce::MidiBuffer& midiMessages);
    void updateArtNetOutput();
    int midiNoteToLEDIndex(int midiNote) const;
    void sendVisualFeedback();
    void sendVisualFeedbackWithRange(int rangeLEDCount);
    void createProtocolSender(int protocol);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Midi2ArtAudioProcessor)
};

