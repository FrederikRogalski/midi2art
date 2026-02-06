/*
  ==============================================================================

    DMXSender.h
    Created: 2024
    Author: Your Name

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Simple ADSR envelope generator (shared by all protocols)
class ADSREnvelope
{
public:
    ADSREnvelope()
    {
        reset();
    }
    
    void setAttack(float seconds) { attackTime = seconds; }
    void setDecay(float seconds) { decayTime = seconds; }
    void setSustain(float level) { sustainLevel = juce::jlimit(0.0f, 1.0f, level); }
    void setRelease(float seconds) { releaseTime = seconds; }
    
    void noteOn()
    {
        state = Attack;
        currentLevel = 0.0f;
        elapsedTime = 0.0f;
    }
    
    void noteOff()
    {
        if (state != Idle)
        {
            state = Release;
            releaseStartLevel = currentLevel;
            elapsedTime = 0.0f;
        }
    }
    
    float getNextValue(float sampleRate)
    {
        const float deltaTime = 1.0f / sampleRate;
        elapsedTime += deltaTime;
        
        switch (state)
        {
            case Idle:
                return 0.0f;
                
            case Attack:
                if (attackTime > 0.0f)
                {
                    // Exponential attack: slow → fast
                    float t = elapsedTime / attackTime;
                    if (t >= 1.0f)
                    {
                        currentLevel = 1.0f;
                        state = Decay;
                        elapsedTime = 0.0f;
                    }
                    else
                    {
                        // Normalized exponential curve: f(0) = 0, f(1) = 1
                        currentLevel = expCurve(t, 0.0f, 1.0f);
                    }
                }
                else
                {
                    currentLevel = 1.0f;
                    state = Decay;
                    elapsedTime = 0.0f;
                }
                return currentLevel;
                
            case Decay:
                if (decayTime > 0.0f)
                {
                    // Exponential decay: fast → slow
                    float t = elapsedTime / decayTime;
                    if (t >= 1.0f)
                    {
                        currentLevel = sustainLevel;
                        state = Sustain;
                        elapsedTime = 0.0f;
                    }
                    else
                    {
                        // Normalized exponential curve: f(0) = 1, f(1) = sustainLevel
                        currentLevel = expCurve(t, 1.0f, sustainLevel);
                    }
                }
                else
                {
                    currentLevel = sustainLevel;
                    state = Sustain;
                    elapsedTime = 0.0f;
                }
                return currentLevel;
                
            case Sustain:
                return sustainLevel;
                
            case Release:
                if (releaseTime > 0.0f)
                {
                    // Exponential release: fast → slow
                    float t = elapsedTime / releaseTime;
                    if (t >= 1.0f)
                    {
                        currentLevel = 0.0f;
                        state = Idle;
                    }
                    else
                    {
                        // Normalized exponential curve: f(0) = releaseStartLevel, f(1) = 0
                        currentLevel = expCurve(t, releaseStartLevel, 0.0f);
                    }
                }
                else
                {
                    currentLevel = 0.0f;
                    state = Idle;
                }
                return currentLevel;
        }
        
        return 0.0f;
    }
    
    bool isActive() const { return state != Idle; }
    
    void reset()
    {
        state = Idle;
        currentLevel = 0.0f;
        elapsedTime = 0.0f;
        releaseStartLevel = 0.0f;
    }
    
private:
    // Normalized exponential curve: f(0) = start, f(1) = end
    // Uses exponential shape but guarantees exact values at boundaries
    static float expCurve(float t, float start, float end, float steepness = 5.0f)
    {
        // Clamp t to [0, 1]
        t = juce::jlimit(0.0f, 1.0f, t);
        
        if (start < end)
        {
            // Rising curve (Attack): exponential rise from start to end
            // Formula: start + (end - start) * (1 - exp(-k*t)) / (1 - exp(-k))
            float k = steepness;
            float exp_kt = std::exp(-k * t);
            float exp_k = std::exp(-k);
            return start + (end - start) * (1.0f - exp_kt) / (1.0f - exp_k);
        }
        else
        {
            // Falling curve (Decay, Release): exponential fall from start to end
            // Formula: end + (start - end) * (exp(-k*t) - exp(-k)) / (1 - exp(-k))
            float k = steepness;
            float exp_kt = std::exp(-k * t);
            float exp_k = std::exp(-k);
            return end + (start - end) * (exp_kt - exp_k) / (1.0f - exp_k);
        }
    }
    
    enum State
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };
    
    State state = Idle;
    float currentLevel = 0.0f;
    float elapsedTime = 0.0f;
    float releaseStartLevel = 0.0f;
    
    float attackTime = 0.01f;
    float decayTime = 0.1f;
    float sustainLevel = 0.7f;
    float releaseTime = 0.2f;
};

// Abstract base class for DMX protocol senders
class DMXSender
{
public:
    // WLED-specific: supports 170 LEDs (510 DMX channels) per universe
    // Standard DMX is 512 channels, but WLED uses 510
    static constexpr int WLED_LEDS_PER_UNIVERSE = 170; // Maximum LEDs per universe
    static constexpr int WLED_CHANNELS_PER_UNIVERSE = 510; // 170 LEDs * 3 RGB channels
    
    virtual ~DMXSender() = default;
    
    // Set target IP address (protocol-specific interpretation)
    virtual void setTargetIP(const juce::String& ipAddress) = 0;
    
    // Set universe number (protocol-specific range)
    virtual void setUniverse(int universe) = 0;
    
    // Send DMX data (splits across universes if needed)
    virtual void sendDMX(const uint8_t* dmxData, int numChannels) = 0;
    
    // Send visual feedback pattern (bright edges, dim middle)
    // Implemented in base class - uses sendDMX() which is protocol-specific
    void sendVisualFeedbackPattern(int numLEDs, int offset, int maxLEDs)
    {
        if (numLEDs == 0)
            return;
        
        // Calculate the actual range we need to cover
        // Pattern starts at offset and extends for numLEDs
        int patternStart = offset;
        int patternEnd = offset + numLEDs - 1;
        int totalLEDs = juce::jmax(maxLEDs, patternEnd + 1);
        
        const int numChannels = totalLEDs * 3; // RGB per LED
        if (numChannels > MAX_FEEDBACK_BUFFER_SIZE)
            return;
        
        // Use pre-allocated buffer (no heap allocation on audio thread)
        memset(feedbackBuffer, 0, numChannels);
        
        if (numLEDs == 1)
        {
            // Single LED - make it bright red at offset position
            int channelIndex = patternStart * 3;
            if (channelIndex + 2 < numChannels)
            {
                feedbackBuffer[channelIndex] = 255;     // R
                feedbackBuffer[channelIndex + 1] = 0;   // G
                feedbackBuffer[channelIndex + 2] = 0;   // B
            }
        }
        else if (numLEDs == 2)
        {
            // Two LEDs - both bright red at offset positions
            for (int i = 0; i < 2; i++)
            {
                int ledPos = patternStart + i;
                int channelIndex = ledPos * 3;
                if (channelIndex + 2 < numChannels)
                {
                    feedbackBuffer[channelIndex] = 255;     // R
                    feedbackBuffer[channelIndex + 1] = 0;   // G
                    feedbackBuffer[channelIndex + 2] = 0;   // B
                }
            }
        }
        else
        {
            // Multiple LEDs - bright red at edges, dim in middle
            // First LED (at offset) - bright red
            int firstChannelIndex = patternStart * 3;
            if (firstChannelIndex + 2 < numChannels)
            {
                feedbackBuffer[firstChannelIndex] = 255;     // R
                feedbackBuffer[firstChannelIndex + 1] = 0;   // G
                feedbackBuffer[firstChannelIndex + 2] = 0;   // B
            }
            
            // Last LED (at offset + numLEDs - 1) - bright red
            int lastChannelIndex = patternEnd * 3;
            if (lastChannelIndex + 2 < numChannels)
            {
                feedbackBuffer[lastChannelIndex] = 255;     // R
                feedbackBuffer[lastChannelIndex + 1] = 0;   // G
                feedbackBuffer[lastChannelIndex + 2] = 0;   // B
            }
            
            // Inner LEDs - dim (about 10% brightness)
            const uint8_t dimValue = 25; // ~10% of 255
            for (int i = 1; i < numLEDs - 1; i++)
            {
                int ledPos = patternStart + i;
                int channelIndex = ledPos * 3;
                if (channelIndex + 2 < numChannels)
                {
                    feedbackBuffer[channelIndex] = dimValue;     // R
                    feedbackBuffer[channelIndex + 1] = dimValue; // G
                    feedbackBuffer[channelIndex + 2] = dimValue; // B
                }
            }
        }
        
        sendDMX(feedbackBuffer, numChannels);
    }
    
    // Send all LEDs off
    // Implemented in base class - uses sendDMX() which is protocol-specific
    void sendAllLEDsOff(int numLEDs)
    {
        if (numLEDs == 0)
            return;
        
        const int numChannels = numLEDs * 3; // RGB per LED
        if (numChannels > MAX_FEEDBACK_BUFFER_SIZE)
            return;
        
        memset(feedbackBuffer, 0, numChannels);
        sendDMX(feedbackBuffer, numChannels);
    }
    
private:
    // Pre-allocated buffer for visual feedback patterns (avoids heap allocation on audio thread)
    static constexpr int MAX_FEEDBACK_BUFFER_SIZE = 512 * 3;
    uint8_t feedbackBuffer[MAX_FEEDBACK_BUFFER_SIZE] = {0};
};

