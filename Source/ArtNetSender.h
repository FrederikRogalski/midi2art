/*
  ==============================================================================

    ArtNetSender.h
    Created: 2024
    Author: Your Name

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DMXSender.h"

// Art-Net packet structure
struct ArtNetPacket
{
    static constexpr int HEADER_SIZE = 18;
    static constexpr int MAX_DMX_CHANNELS = 512;
    static constexpr int MAX_PACKET_SIZE = HEADER_SIZE + MAX_DMX_CHANNELS + 1;
    
    // Art-Net packet header
    char id[8] = {'A', 'r', 't', '-', 'N', 'e', 't', 0};
    uint16_t opCode = 0x5000; // OpDmx (little-endian)
    uint16_t protocolVersion = 14; // Art-Net protocol version
    uint8_t sequence = 0;
    uint8_t physical = 0;
    uint16_t universe = 0; // Universe (little-endian)
    uint16_t dataLength = 0; // Length of DMX data (little-endian)
    uint8_t data[MAX_DMX_CHANNELS];
    
    // Convert to byte array for sending
    void toByteArray(juce::MemoryBlock& block) const
    {
        block.setSize(HEADER_SIZE + dataLength, false);
        auto* data = static_cast<uint8_t*>(block.getData());
        
        // Copy header ID
        juce::zeromem(data, 8);
        memcpy(data, id, 7); // Copy "Art-Net" (7 chars) + null terminator
        data += 8;
        
        // OpCode (little-endian) - 0x5000 = OpDmx
        *data++ = static_cast<uint8_t>(opCode & 0xFF);
        *data++ = static_cast<uint8_t>((opCode >> 8) & 0xFF);
        
        // Protocol version (little-endian) - 14
        *data++ = static_cast<uint8_t>(protocolVersion & 0xFF);
        *data++ = static_cast<uint8_t>((protocolVersion >> 8) & 0xFF);
        
        // Sequence and physical
        *data++ = sequence;
        *data++ = physical;
        
        // Universe (little-endian)
        *data++ = static_cast<uint8_t>(universe & 0xFF);
        *data++ = static_cast<uint8_t>((universe >> 8) & 0xFF);
        
        // Data length (big-endian) - high byte first, then low byte (Art-Net spec)
        *data++ = static_cast<uint8_t>((dataLength >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(dataLength & 0xFF);
        
        // Copy DMX data
        memcpy(data, this->data, this->dataLength);
    }
};

// Art-Net sender class
class ArtNetSender : public DMXSender
{
public:
    ArtNetSender()
    {
        socket.bindToPort(0); // Bind to any available port
    }
    
    ~ArtNetSender()
    {
        socket.shutdown();
    }
    
    void setTargetIP(const juce::String& ipAddress) override
    {
        targetIP = ipAddress;
    }
    
    void setUniverse(int universe) override
    {
        currentUniverse = universe;
    }
    
    void sendDMX(const uint8_t* dmxData, int numChannels) override
    {
        if (targetIP.isEmpty() || numChannels == 0)
            return;
        
        // Split across multiple universes if needed (WLED uses 510 channels per universe)
        int channelsRemaining = numChannels;
        int universeOffset = currentUniverse;
        int offset = 0;
        
        while (channelsRemaining > 0)
        {
            int channelsInThisPacket = juce::jmin(channelsRemaining, WLED_CHANNELS_PER_UNIVERSE);
            
            ArtNetPacket packet;
            packet.universe = universeOffset;
            packet.dataLength = static_cast<uint16_t>(channelsInThisPacket);
            
            memcpy(packet.data, dmxData + offset, packet.dataLength);
            
            juce::MemoryBlock block;
            packet.toByteArray(block);
            
            socket.write(targetIP, 6454, block.getData(), static_cast<int>(block.getSize()));
            
            channelsRemaining -= channelsInThisPacket;
            offset += channelsInThisPacket;
            universeOffset++;
        }
    }
    
    // sendVisualFeedbackPattern and sendAllLEDsOff are implemented in base class DMXSender
    
private:
    juce::DatagramSocket socket;
    juce::String targetIP;
    int currentUniverse = 0;
};

