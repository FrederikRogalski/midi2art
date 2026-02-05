/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
Midi2ArtAudioProcessor::Midi2ArtAudioProcessor()
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
    parameters(*this, nullptr, juce::Identifier("Midi2ArtParams"),
               {
                   std::make_unique<juce::AudioParameterInt>(PARAM_LED_COUNT, "LED Count", 1, 512, 88),
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
                   std::make_unique<juce::AudioParameterInt>(PARAM_PROTOCOL, "Protocol", 0, 1, 1),  // 0 = Art-Net, 1 = E1.31
                   std::make_unique<juce::AudioParameterInt>(PARAM_UNIVERSE, "Universe", 0, 63999, 1)  // WLED default start universe is 1
               })
{
    previousLEDCount = *parameters.getRawParameterValue(PARAM_LED_COUNT);
    
    // Initialize IP address in ValueTree (E1.31 Multicast Universe 1)
    parameters.state.setProperty(PARAM_WLED_IP, "239.255.0.1", nullptr);
    
    // Initialize protocol sender (default: E1.31)
    createProtocolSender(1);
}

Midi2ArtAudioProcessor::~Midi2ArtAudioProcessor()
{
}

//==============================================================================
const juce::String Midi2ArtAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Midi2ArtAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Midi2ArtAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Midi2ArtAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Midi2ArtAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Midi2ArtAudioProcessor::getNumPrograms()
{
    return 1;
}

int Midi2ArtAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Midi2ArtAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Midi2ArtAudioProcessor::getProgramName (int index)
{
    return {};
}

void Midi2ArtAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Midi2ArtAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    updateCounter = 0;
}

void Midi2ArtAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Midi2ArtAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void Midi2ArtAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Update parameters
    updateParameters();
    
    // Process MIDI messages
    processMidiMessages(midiMessages);
    
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
    
    // Send Art-Net periodically when there are active notes (to reflect ADSR changes)
    // This allows ADSR envelope changes to be visible in real-time
    // Only send when notes are active to avoid interference with multiple plugin instances
    if (activeNotes.size() > 0)
    {
        updateCounter += numSamples;
        if (updateCounter >= UPDATE_INTERVAL)
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
bool Midi2ArtAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* Midi2ArtAudioProcessor::createEditor()
{
    return new Midi2ArtAudioProcessorEditor (*this);
}

//==============================================================================
void Midi2ArtAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void Midi2ArtAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
        }
}

//==============================================================================
void Midi2ArtAudioProcessor::updateParameters()
{
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
        
        // Track maximum LED count ever set
        if (currentLEDCount > maxLEDCountEver)
        {
            maxLEDCountEver = currentLEDCount;
        }
        
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
    
    // Update protocol
    int newProtocol = static_cast<int>(*parameters.getRawParameterValue(PARAM_PROTOCOL));
    if (newProtocol != currentProtocol)
    {
        currentProtocol = newProtocol;
        createProtocolSender(currentProtocol);
        // Update IP and universe on new sender
        if (dmxSender)
        {
            dmxSender->setTargetIP(currentWLEDIP);
            dmxSender->setUniverse(currentUniverse);
        }
    }
    
    // Update universe
    int newUniverse = static_cast<int>(*parameters.getRawParameterValue(PARAM_UNIVERSE));
    if (newUniverse != currentUniverse)
    {
        currentUniverse = newUniverse;
        if (dmxSender)
        {
            dmxSender->setUniverse(currentUniverse);
        }
    }
    
    // Update WLED IP (from ValueTree property)
    juce::String newIP = parameters.state.getProperty(PARAM_WLED_IP, "239.255.0.1").toString();
    if (newIP != currentWLEDIP)
    {
        currentWLEDIP = newIP;
        if (dmxSender)
        {
            dmxSender->setTargetIP(currentWLEDIP);
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

void Midi2ArtAudioProcessor::processMidiMessages(juce::MidiBuffer& midiMessages)
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
                continue; // Ignore notes outside the range
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
    
    // Remove inactive notes and check if we need to update
    int numBefore = activeNotes.size();
    activeNotes.removeIf([](const ActiveNote& note) { return !note.envelope.isActive(); });
    if (activeNotes.size() != numBefore)
    {
        notesChanged = true;
        // Notify UI that active notes count changed
        sendChangeMessage();
    }
    
    // Send Art-Net update only when MIDI events occur (event-based, not continuous)
    // This prevents interference when multiple plugin instances are running
    if (notesChanged)
    {
        updateArtNetOutput();
    }
}

void Midi2ArtAudioProcessor::updateArtNetOutput()
{
    if (currentLEDCount == 0 || currentWLEDIP.isEmpty())
    {
        return;
    }
    
    // Calculate the actual range we need to cover
    // Pattern extends from offset to offset + count - 1
    int patternEnd = currentLEDOffset + currentLEDCount - 1;
    int packetLEDCount = juce::jmax(patternEnd + 1, maxLEDCountEver);
    const int numChannels = packetLEDCount * 3; // RGB per LED
    juce::HeapBlock<uint8_t> dmxData(numChannels);
    
    // Initialize all channels to zero (this ensures LEDs beyond the pattern are off)
    juce::zeromem(dmxData, numChannels);
    
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
                dmxData[channelIndex] = r;
                dmxData[channelIndex + 1] = g;
                dmxData[channelIndex + 2] = b;
            }
        }
    }
    
    if (dmxSender)
    {
        dmxSender->sendDMX(dmxData, numChannels);
    }
}

int Midi2ArtAudioProcessor::midiNoteToLEDIndex(int midiNote) const
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

void Midi2ArtAudioProcessor::sendVisualFeedback()
{
    // Send visual feedback pattern: bright edges, dim middle
    // This makes it easy to see the boundaries even when exceeding actual LED count
    // Calculate the range we need to cover (pattern + any previous range)
    int patternEnd = currentLEDOffset + currentLEDCount - 1;
    int maxLEDs = juce::jmax(maxLEDCountEver, patternEnd + 1);
    if (dmxSender)
    {
        dmxSender->sendVisualFeedbackPattern(currentLEDCount, currentLEDOffset, maxLEDs);
    }
}

void Midi2ArtAudioProcessor::sendVisualFeedbackWithRange(int rangeLEDCount)
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
void Midi2ArtAudioProcessor::createProtocolSender(int protocol)
{
    // Destroy existing sender
    dmxSender.reset();
    
    // Create new sender based on protocol
    if (protocol == 0)
    {
        // Art-Net
        dmxSender = std::make_unique<ArtNetSender>();
    }
    else if (protocol == 1)
    {
        // E1.31 (sACN)
        dmxSender = std::make_unique<E131Sender>();
    }
    else
    {
        // Default to E1.31 if unknown protocol
        dmxSender = std::make_unique<E131Sender>();
    }
    
    // Initialize with current settings
    if (dmxSender)
    {
        dmxSender->setTargetIP(currentWLEDIP);
        dmxSender->setUniverse(currentUniverse);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Midi2ArtAudioProcessor();
}

