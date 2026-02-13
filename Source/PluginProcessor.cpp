/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
KeyGlowAudioProcessor::KeyGlowAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output",  juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    parameters(*this, nullptr, juce::Identifier("KeyGlowParams"),
               {
                   std::make_unique<juce::AudioParameterInt>(PARAM_LED_COUNT, "LED Count", 1, 512, 74),
                   std::make_unique<juce::AudioParameterInt>(PARAM_LED_OFFSET, "LED Offset", 0, DMXSender::WLED_LEDS_PER_UNIVERSE, 0),  // Max 170 (LEDs per universe)
                   std::make_unique<juce::AudioParameterInt>(PARAM_LOWEST_NOTE, "Lowest Note", 0, 127, 21),  // A0
                   std::make_unique<juce::AudioParameterInt>(PARAM_HIGHEST_NOTE, "Highest Note", 0, 127, 108),  // C8
                   std::make_unique<juce::AudioParameterFloat>(PARAM_ATTACK, "Attack", 
                       juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 0.1f),
                   std::make_unique<juce::AudioParameterFloat>(PARAM_DECAY, "Decay",
                       juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 0.7f),
                   std::make_unique<juce::AudioParameterFloat>(PARAM_SUSTAIN, "Sustain",
                       juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.1f),
                   std::make_unique<juce::AudioParameterFloat>(PARAM_RELEASE, "Release",
                       juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f), 0.2f),
                   std::make_unique<juce::AudioParameterFloat>(PARAM_COLOR_HUE, "Color Hue",
                       juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.667f),  // Blue (240° / 360°)
                   std::make_unique<juce::AudioParameterFloat>(PARAM_COLOR_SAT, "Color Saturation",
                       juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f),
                   std::make_unique<juce::AudioParameterFloat>(PARAM_COLOR_VAL, "Color Value",
                       juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f),
                   std::make_unique<juce::AudioParameterInt>(PARAM_PROTOCOL, "Protocol", 0, 2, 2),  // 0 = Art-Net, 1 = E1.31, 2 = Adalight
                   std::make_unique<juce::AudioParameterInt>(PARAM_UNIVERSE, "Universe", 0, 63999, 1),  // Network protocols only (Art-Net, E1.31)
                    std::make_unique<juce::AudioParameterInt>(PARAM_BAUD_RATE, "Baud Rate", 57600, 921600, 115200)  // Adalight serial only
               })
{
    previousLEDCount = *parameters.getRawParameterValue(PARAM_LED_COUNT);
    
    // Initialize ValueTree properties only if not already set (preserves saved state)
    if (!parameters.state.hasProperty(PARAM_WLED_IP))
        parameters.state.setProperty(PARAM_WLED_IP, "239.255.0.1", nullptr);
    
    if (!parameters.state.hasProperty(PARAM_SERIAL_PORT))
        parameters.state.setProperty(PARAM_SERIAL_PORT, "", nullptr);
    
    // Read saved state into member variables BEFORE creating the sender
    currentProtocol = static_cast<int>(*parameters.getRawParameterValue(PARAM_PROTOCOL));
    currentWLEDIP = parameters.state.getProperty(PARAM_WLED_IP, "239.255.0.1").toString();
    currentSerialPort = parameters.state.getProperty(PARAM_SERIAL_PORT, "").toString();
    currentUniverse = static_cast<int>(*parameters.getRawParameterValue(PARAM_UNIVERSE));
    currentBaudRate = static_cast<int>(*parameters.getRawParameterValue(PARAM_BAUD_RATE));
    currentLEDCount = static_cast<int>(*parameters.getRawParameterValue(PARAM_LED_COUNT));
    currentLEDOffset = static_cast<int>(*parameters.getRawParameterValue(PARAM_LED_OFFSET));
    
    // Initialize protocol sender with the saved (or default) protocol
    createProtocolSender(currentProtocol);
}

KeyGlowAudioProcessor::~KeyGlowAudioProcessor()
{
}

//==============================================================================
const juce::String KeyGlowAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KeyGlowAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KeyGlowAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KeyGlowAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KeyGlowAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KeyGlowAudioProcessor::getNumPrograms()
{
    return 1;
}

int KeyGlowAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KeyGlowAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KeyGlowAudioProcessor::getProgramName (int index)
{
    return {};
}

void KeyGlowAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void KeyGlowAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    updateCounter = 0;
    updateInterval = static_cast<int>(sampleRate / TARGET_UPDATE_HZ);
}

void KeyGlowAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KeyGlowAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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
#endif

void KeyGlowAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Update parameters
    updateParameters();
    
    // Process MIDI messages
    if (midiMessages.getNumEvents() > 0)
    {
        processMidiMessages(midiMessages);
    }
    
    // Update ADSR envelopes for active notes
    // We update envelopes at audio rate for smooth transitions
    const int numSamples = buffer.getNumSamples();
    for (int i = 0; i < numSamples; i++)
    {
        for (auto& note : activeNotes)
        {
            if (note.envelope.isActive())
            {
                note.currentEnvelopeLevel = note.envelope.getNextValue(static_cast<float>(sampleRate));
            }
            else
            {
                note.currentEnvelopeLevel = 0.0f;
            }
        }
    }
    
    // Remove notes whose envelopes have finished (reached Idle state)
    // This must happen here in processBlock, not just in processMidiMessages,
    // because envelopes can finish their release phase between MIDI events
    int numBefore = activeNotes.size();
    activeNotes.removeIf([](const ActiveNote& note) { return !note.envelope.isActive(); });
    if (activeNotes.size() != numBefore)
    {
        sendChangeMessage();
    }
    
    // Send to LEDs periodically when there are active notes (to reflect ADSR changes)
    // This allows ADSR envelope changes to be visible in real-time
    // Only send when notes are active to avoid interference with multiple plugin instances
    if (activeNotes.size() > 0)
    {
        updateCounter += numSamples;
        if (updateCounter >= updateInterval)
        {
            updateArtNetOutput();
            updateCounter = 0;
        }
    }
    else
    {
        // Reset counter when no notes are active
        updateCounter = 0;
    }
    
    // Clear buffer (this is a MIDI effect, no audio processing)
    buffer.clear();
}

//==============================================================================
bool KeyGlowAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* KeyGlowAudioProcessor::createEditor()
{
    return new KeyGlowAudioProcessorEditor (*this);
}

//==============================================================================
void KeyGlowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void KeyGlowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
        }
}

//==============================================================================
void KeyGlowAudioProcessor::updateParameters()
{
    // Update protocol FIRST - ensures the correct sender is active
    // before any visual feedback or data is sent
    int newProtocol = static_cast<int>(*parameters.getRawParameterValue(PARAM_PROTOCOL));
    if (newProtocol != currentProtocol)
    {
        DBG("PluginProcessor::updateParameters - Protocol changed from " + juce::String(currentProtocol) + " to " + juce::String(newProtocol));
        currentProtocol = newProtocol;
        createProtocolSender(currentProtocol);
    }
    
    // Update universe (for network protocols)
    int newUniverse = static_cast<int>(*parameters.getRawParameterValue(PARAM_UNIVERSE));
    if (newUniverse != currentUniverse)
    {
        currentUniverse = newUniverse;
        if (dmxSender && currentProtocol != 2)  // Not Adalight
        {
            dmxSender->setUniverse(currentUniverse);
        }
    }
    
    // Update baud rate (for Adalight)
    int newBaudRate = static_cast<int>(*parameters.getRawParameterValue(PARAM_BAUD_RATE));
    if (newBaudRate != currentBaudRate)
    {
        currentBaudRate = newBaudRate;
        if (dmxSender && currentProtocol == 2)  // Adalight only
        {
            dmxSender->setUniverse(currentBaudRate);  // For Adalight, setUniverse() configures baud rate
        }
    }
    
    // Update WLED IP (from ValueTree property) - for network protocols
    juce::String newIP = parameters.state.getProperty(PARAM_WLED_IP, "239.255.0.1").toString();
    if (newIP != currentWLEDIP)
    {
        currentWLEDIP = newIP;
        if (dmxSender && currentProtocol != 2) // Not Adalight
        {
            dmxSender->setTargetIP(currentWLEDIP);
        }
    }
    
    // Update Serial Port (from ValueTree property) - for Adalight
    juce::String newSerialPort = parameters.state.getProperty(PARAM_SERIAL_PORT, "").toString();
    if (newSerialPort != currentSerialPort)
    {
        DBG("PluginProcessor::updateParameters - Serial port changed from '" + currentSerialPort + "' to '" + newSerialPort + "'");
        currentSerialPort = newSerialPort;
        if (dmxSender && currentProtocol == 2) // Adalight
        {
            DBG("  Calling setTargetIP with new serial port: '" + currentSerialPort + "'");
            dmxSender->setTargetIP(currentSerialPort); // setTargetIP is used for serial port name in Adalight
        }
    }
    
    // Update LED offset
    int newLEDOffset = static_cast<int>(*parameters.getRawParameterValue(PARAM_LED_OFFSET));
    if (newLEDOffset != currentLEDOffset)
    {
        int oldOffset = currentLEDOffset;
        currentLEDOffset = newLEDOffset;
        
        // Send visual feedback when offset changes to show the new range
        // Calculate the range we need to cover (old and new positions)
        int patternEnd = juce::jmax(oldOffset + currentLEDCount - 1, newLEDOffset + currentLEDCount - 1);
        int rangeLEDCount = patternEnd + 1;
        sendVisualFeedbackWithRange(rangeLEDCount);
    }
    
    // Update note range
    int newLowestNote = static_cast<int>(*parameters.getRawParameterValue(PARAM_LOWEST_NOTE));
    int newHighestNote = static_cast<int>(*parameters.getRawParameterValue(PARAM_HIGHEST_NOTE));
    if (newLowestNote != currentLowestNote || newHighestNote != currentHighestNote)
    {
        // Ensure lowest <= highest
        if (newLowestNote > newHighestNote)
        {
            if (newLowestNote != currentLowestNote)
                newLowestNote = newHighestNote;
            else
                newHighestNote = newLowestNote;
        }
        currentLowestNote = newLowestNote;
        currentHighestNote = newHighestNote;
    }
    
    // Update LED count
    int newLEDCount = static_cast<int>(*parameters.getRawParameterValue(PARAM_LED_COUNT));
    if (newLEDCount != currentLEDCount)
    {
        int oldLEDCount = currentLEDCount;
        currentLEDCount = newLEDCount;
        
        // Send visual feedback when LED count changes
        if (previousLEDCount != currentLEDCount)
        {
            // Calculate the range we need to cover (old and new positions)
            int oldPatternEnd = currentLEDOffset + oldLEDCount - 1;
            int newPatternEnd = currentLEDOffset + newLEDCount - 1;
            int rangeLEDCount = juce::jmax(oldPatternEnd + 1, newPatternEnd + 1);
            
            // Always use sendVisualFeedbackWithRange to ensure proper cleanup
            sendVisualFeedbackWithRange(rangeLEDCount);
            previousLEDCount = currentLEDCount;
        }
    }
    
    // Update ADSR parameters
    attackTime = *parameters.getRawParameterValue(PARAM_ATTACK);
    decayTime = *parameters.getRawParameterValue(PARAM_DECAY);
    sustainLevel = *parameters.getRawParameterValue(PARAM_SUSTAIN);
    releaseTime = *parameters.getRawParameterValue(PARAM_RELEASE);
    
    // Update color
    float hue = *parameters.getRawParameterValue(PARAM_COLOR_HUE);
    float sat = *parameters.getRawParameterValue(PARAM_COLOR_SAT);
    float val = *parameters.getRawParameterValue(PARAM_COLOR_VAL);
    currentColor = juce::Colour::fromHSV(hue, sat, val, 1.0f);
    
    // Update envelopes for active notes
    for (auto& note : activeNotes)
    {
        note.envelope.setAttack(attackTime);
        note.envelope.setDecay(decayTime);
        note.envelope.setSustain(sustainLevel);
        note.envelope.setRelease(releaseTime);
        note.color = currentColor;
    }
}

void KeyGlowAudioProcessor::processMidiMessages(juce::MidiBuffer& midiMessages)
{
    bool notesChanged = false;
    
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        
        if (message.isNoteOn())
        {
            int midiNote = message.getNoteNumber();
            
            // Handle MIDI learn
            if (midiLearnState == MidiLearnState::LearningLowestNote)
            {
                parameters.getParameter(PARAM_LOWEST_NOTE)->setValueNotifyingHost(
                    parameters.getParameter(PARAM_LOWEST_NOTE)->convertTo0to1(midiNote));
                midiLearnState = MidiLearnState::None;
                sendChangeMessage(); // Notify UI that MIDI learn state changed
                continue; // Don't process as a regular note
            }
            else if (midiLearnState == MidiLearnState::LearningHighestNote)
            {
                parameters.getParameter(PARAM_HIGHEST_NOTE)->setValueNotifyingHost(
                    parameters.getParameter(PARAM_HIGHEST_NOTE)->convertTo0to1(midiNote));
                midiLearnState = MidiLearnState::None;
                sendChangeMessage(); // Notify UI that MIDI learn state changed
                continue; // Don't process as a regular note
            }
            
            // Check if note is in range
            if (midiNote < currentLowestNote || midiNote > currentHighestNote)
            {
                continue;
            }
            
            float velocity = message.getFloatVelocity();
            int ledIndex = midiNoteToLEDIndex(midiNote);
            
            // Check if note is already active
            bool found = false;
            for (auto& note : activeNotes)
            {
                if (note.midiNote == midiNote)
                {
                    // Re-trigger the note
                    note.velocity = velocity;
                    note.ledIndex = ledIndex;
                    note.color = currentColor;
                    note.isSustained = false; // Reset sustain state
                    note.envelope.setAttack(attackTime);
                    note.envelope.setDecay(decayTime);
                    note.envelope.setSustain(sustainLevel);
                    note.envelope.setRelease(releaseTime);
                    note.envelope.noteOn();
                    // Update envelope level once immediately so Art-Net packet has a value
                    note.currentEnvelopeLevel = note.envelope.getNextValue(static_cast<float>(sampleRate));
                    found = true;
                    notesChanged = true;
                    break;
                }
            }
            
            if (!found)
            {
                int numBefore = activeNotes.size();
                ActiveNote newNote;
                newNote.midiNote = midiNote;
                newNote.ledIndex = ledIndex;
                newNote.velocity = velocity;
                newNote.color = currentColor;
                newNote.currentEnvelopeLevel = 0.0f;
                newNote.isSustained = false;
                newNote.envelope.setAttack(attackTime);
                newNote.envelope.setDecay(decayTime);
                newNote.envelope.setSustain(sustainLevel);
                newNote.envelope.setRelease(releaseTime);
                newNote.envelope.noteOn();
                // Update envelope level once immediately so Art-Net packet has a value
                newNote.currentEnvelopeLevel = newNote.envelope.getNextValue(static_cast<float>(sampleRate));
                activeNotes.add(newNote);
                notesChanged = true;
                // Notify UI that active notes count changed
                if (activeNotes.size() != numBefore)
                {
                    sendChangeMessage();
                }
            }
        }
                else if (message.isNoteOff())
        {
            int midiNote = message.getNoteNumber();
            
            // Check if note is in range (or was previously active)
            // We still process note-off even if outside range to clean up any active notes
            for (auto& note : activeNotes)
            {
                if (note.midiNote == midiNote)
                {
                    if (sustainPedalActive)
                    {
                        // Sustain pedal is active - hold the note in sustain phase
                        note.isSustained = true;
                    }
                    else
                    {
                        // No sustain - release the note normally
                        note.envelope.noteOff();
                        note.isSustained = false;
                        // Update envelope level once immediately so Art-Net packet reflects release state
                        note.currentEnvelopeLevel = note.envelope.getNextValue(static_cast<float>(sampleRate));
                    }
                    notesChanged = true;
                    break;
                }
            }
        }
        else if (message.isControllerOfType(64)) // Sustain pedal (CC 64)
        {
            bool newSustainState = message.getControllerValue() >= 64;
            
            if (newSustainState != sustainPedalActive)
            {
                sustainPedalActive = newSustainState;
                
                if (!sustainPedalActive)
                {
                    // Sustain pedal released - release all sustained notes
                    for (auto& note : activeNotes)
                    {
                        if (note.isSustained)
                        {
                            note.envelope.noteOff();
                            note.isSustained = false;
                            // Update envelope level once immediately
                            note.currentEnvelopeLevel = note.envelope.getNextValue(static_cast<float>(sampleRate));
                        }
                    }
                }
                notesChanged = true;
            }
        }
    }
    
    // Note: inactive note removal is handled in processBlock() after envelope updates,
    // so it catches notes that finish their release between MIDI events.
    
    // Send Art-Net update only when MIDI events occur (event-based, not continuous)
    // This prevents interference when multiple plugin instances are running
    // NOTE: For Adalight, the timer-based sending at 30fps is sufficient.
    // Event-driven sending was designed for network protocols where bandwidth isn't an issue.
    if (notesChanged && currentProtocol != 2)
    {
        updateArtNetOutput();
    }
}

void KeyGlowAudioProcessor::updateArtNetOutput()
{
    // Calculate the actual range we need to cover
    // Packet covers LEDs from 0 to (offset + count - 1)
    int packetLEDCount = currentLEDOffset + currentLEDCount;
    const int numChannels = packetLEDCount * 3; // RGB per LED
    
    // Clamp to pre-allocated buffer size
    if (numChannels > MAX_DMX_BUFFER_SIZE || numChannels == 0)
        return;
    
    // Use pre-allocated buffer (no heap allocation on audio thread)
    // Initialize all channels to zero (ensures LEDs beyond the pattern are off)
    memset(dmxBuffer, 0, numChannels);
    
    // Set LED values based on active notes
    for (const auto& note : activeNotes)
    {
        // Check if LED index is within the valid range (offset to offset + count)
        int minLEDIndex = currentLEDOffset;
        int maxLEDIndex = currentLEDOffset + currentLEDCount - 1;
        
        if (note.ledIndex >= minLEDIndex && note.ledIndex <= maxLEDIndex && note.envelope.isActive())
        {
            // Use the stored envelope level (updated in processBlock)
            float brightness = note.currentEnvelopeLevel * note.velocity;
            
            // Get RGB values from color
            uint8_t r = static_cast<uint8_t>(note.color.getRed() * brightness);
            uint8_t g = static_cast<uint8_t>(note.color.getGreen() * brightness);
            uint8_t b = static_cast<uint8_t>(note.color.getBlue() * brightness);
            
            int channelIndex = note.ledIndex * 3;
            if (channelIndex + 2 < numChannels)
            {
                dmxBuffer[channelIndex] = r;
                dmxBuffer[channelIndex + 1] = g;
                dmxBuffer[channelIndex + 2] = b;
            }
        }
    }
    
    if (dmxSender)
    {
        dmxSender->sendDMX(dmxBuffer, numChannels);
    }
}

int KeyGlowAudioProcessor::midiNoteToLEDIndex(int midiNote) const
{
    if (currentLEDCount == 0)
        return 0;
    
    // Clamp note to range
    int clampedNote = juce::jlimit(currentLowestNote, currentHighestNote, midiNote);
    
    // Map note range to LED range
    int noteRange = currentHighestNote - currentLowestNote;
    if (noteRange == 0)
        return currentLEDOffset; // Single note maps to offset position
    
    // Calculate note position within the range (0 to noteRange)
    int notePosition = clampedNote - currentLowestNote;
    
    // Map to LED index using integer division with proper rounding
    // We want: position 0 → LED 0, position noteRange → LED (currentLEDCount - 1)
    // Formula: (notePosition * (currentLEDCount - 1) + noteRange/2) / noteRange
    // This ensures even distribution: divides note range into currentLEDCount equal segments
    // The +noteRange/2 provides rounding to nearest LED
    int ledIndex = (notePosition * (currentLEDCount - 1) + noteRange / 2) / noteRange;
    
    // Clamp to valid LED range (safety check - should always be in range with correct formula)
    ledIndex = juce::jlimit(0, currentLEDCount - 1, ledIndex);
    
    // Add offset
    return ledIndex + currentLEDOffset;
}

void KeyGlowAudioProcessor::sendVisualFeedback()
{
    // Send visual feedback pattern: bright edges, dim middle
    int patternEnd = currentLEDOffset + currentLEDCount - 1;
    int maxLEDs = patternEnd + 1;
    if (dmxSender)
    {
        dmxSender->sendVisualFeedbackPattern(currentLEDCount, currentLEDOffset, maxLEDs);
    }
}

void KeyGlowAudioProcessor::sendVisualFeedbackWithRange(int rangeLEDCount)
{
    // Send visual feedback pattern for currentLEDCount at currentLEDOffset, 
    // but in a packet covering rangeLEDCount
    // This ensures LEDs beyond the pattern are explicitly set to zero (no jitter)
    if (dmxSender)
    {
        dmxSender->sendVisualFeedbackPattern(currentLEDCount, currentLEDOffset, rangeLEDCount);
    }
}

//==============================================================================
void KeyGlowAudioProcessor::createProtocolSender(int protocol)
{
    DBG("============================================");
    DBG("PluginProcessor::createProtocolSender - PROTOCOL SWITCH to: " + juce::String(protocol));
    DBG("  Active notes: " + juce::String(activeNotes.size()));
    
    // Destroy existing sender
    dmxSender.reset();
    DBG("  Old sender destroyed");
    
    // Create new sender based on protocol
    if (protocol == 0)
    {
        // Art-Net
        dmxSender = std::make_unique<ArtNetSender>();
        DBG("  Created ArtNetSender");
    }
    else if (protocol == 1)
    {
        // E1.31 (sACN)
        dmxSender = std::make_unique<E131Sender>();
        DBG("  Created E131Sender");
    }
    else if (protocol == 2)
    {
        // Adalight (USB Serial)
        dmxSender = std::make_unique<AdalightSender>();
        DBG("  Created AdalightSender");
    }
    else
    {
        // Default to E1.31 if unknown protocol
        dmxSender = std::make_unique<E131Sender>();
        DBG("  Created E131Sender (default/unknown)");
    }
    
    // Re-read current values from ValueTree to avoid stale state
    // This is critical for protocol switching: the member variables may have been
    // set during a different protocol's lifecycle
    currentWLEDIP = parameters.state.getProperty(PARAM_WLED_IP, "239.255.0.1").toString();
    currentSerialPort = parameters.state.getProperty(PARAM_SERIAL_PORT, "").toString();
    
    DBG("  Read from ValueTree - IP: '" + currentWLEDIP + "', Serial: '" + currentSerialPort + "'");
    
    // Initialize with current settings
    if (dmxSender)
    {
        if (protocol == 2)
        {
            // Adalight - use serial port and baud rate
            DBG("  Calling setTargetIP with serial port: '" + currentSerialPort + "'");
            dmxSender->setTargetIP(currentSerialPort);
            DBG("  Calling setUniverse (baud rate) with: " + juce::String(currentBaudRate));
            dmxSender->setUniverse(currentBaudRate);  // For Adalight, this sets the baud rate
        }
        else
        {
            // Network protocol - use IP and universe
            DBG("  Calling setTargetIP with IP: '" + currentWLEDIP + "'");
            dmxSender->setTargetIP(currentWLEDIP);
            DBG("  Calling setUniverse with: " + juce::String(currentUniverse));
            dmxSender->setUniverse(currentUniverse);  // For network protocols, this sets the universe number
        }
        DBG("  Sender initialized");
    }
    DBG("============================================");
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyGlowAudioProcessor();
}

