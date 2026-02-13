/*
  ==============================================================================

    E131Sender.h
    Created: 2024
    Author: Your Name

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DMXSender.h"

// E1.31 (sACN) packet structure
struct E131Packet
{
    static constexpr int ROOT_LAYER_SIZE = 38;
    static constexpr int FRAMING_LAYER_SIZE = 77;
    static constexpr int DMP_LAYER_HEADER_SIZE = 11;
    static constexpr int MAX_DMX_CHANNELS = 512;
    static constexpr int MAX_PACKET_SIZE = ROOT_LAYER_SIZE + FRAMING_LAYER_SIZE + DMP_LAYER_HEADER_SIZE + MAX_DMX_CHANNELS;
    
    // Root Layer (38 bytes)
    uint16_t preambleSize = 0x0010;      // Preamble Size (2 bytes, big-endian)
    uint16_t postambleSize = 0x0000;     // Post-amble Size (2 bytes, big-endian)
    char acnPacketId[12] = {'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', 0, 0, 0}; // ACN Packet Identifier
    uint16_t rootFlagsLength = 0;        // Flags & Length (2 bytes, big-endian)
    uint32_t rootVector = 0x00000004;    // Root Vector (4 bytes, big-endian)
    uint8_t cid[16] = {0};               // CID (Component Identifier, 16 bytes)
    
    // Framing Layer (77 bytes)
    uint16_t framingFlagsLength = 0;     // Flags & Length (2 bytes, big-endian)
    uint32_t framingVector = 0x00000002; // Framing Vector (4 bytes, big-endian)
    char sourceName[64] = {0};           // Source Name (64 bytes)
    uint8_t priority = 100;             // Priority (1 byte, 0-200, default 100)
    uint16_t reserved = 0x0000;          // Reserved (2 bytes)
    uint8_t sequenceNumber = 0;          // Sequence Number (1 byte, 0-255)
    uint8_t options = 0x00;              // Options (1 byte: Preview Data = 0, Stream Terminated = 0)
    uint16_t universe = 0;               // Universe (2 bytes, big-endian)
    
    // DMP Layer (11 + data bytes)
    uint16_t dmpFlagsLength = 0;         // Flags & Length (2 bytes, big-endian)
    uint8_t dmpVector = 0x02;            // DMP Vector (1 byte: DMP Set Property)
    uint8_t addressDataType = 0xA1;      // Address & Data Type (1 byte)
    uint16_t firstPropertyAddress = 0x0000; // First Property Address (2 bytes, big-endian)
    uint16_t addressIncrement = 0x0001;   // Address Increment (2 bytes, big-endian)
    uint16_t propertyValueCount = 0;      // Property Value Count (2 bytes, big-endian: 1 + data length)
    uint8_t dmxStartCode = 0x00;         // DMX Start Code (1 byte)
    uint8_t dmxData[MAX_DMX_CHANNELS];   // DMX Data (up to 512 bytes, but WLED uses 510)
    
    // Convert to byte array for sending
    void toByteArray(juce::MemoryBlock& block, int dataLength) const
    {
        // Calculate packet size
        int packetSize = ROOT_LAYER_SIZE + FRAMING_LAYER_SIZE + DMP_LAYER_HEADER_SIZE + dataLength;
        block.setSize(packetSize, false);
        auto* data = static_cast<uint8_t*>(block.getData());
        
        // Root Layer (38 bytes)
        // Preamble Size (big-endian)
        *data++ = static_cast<uint8_t>((preambleSize >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(preambleSize & 0xFF);
        
        // Post-amble Size (big-endian)
        *data++ = static_cast<uint8_t>((postambleSize >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(postambleSize & 0xFF);
        
        // ACN Packet Identifier
        memcpy(data, acnPacketId, 12);
        data += 12;
        
        // Root Flags & Length (big-endian)
        uint16_t rootLength = ROOT_LAYER_SIZE - 16 + FRAMING_LAYER_SIZE + DMP_LAYER_HEADER_SIZE + dataLength;
        *data++ = static_cast<uint8_t>((rootLength >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(rootLength & 0xFF);
        
        // Root Vector (big-endian)
        *data++ = static_cast<uint8_t>((rootVector >> 24) & 0xFF);
        *data++ = static_cast<uint8_t>((rootVector >> 16) & 0xFF);
        *data++ = static_cast<uint8_t>((rootVector >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(rootVector & 0xFF);
        
        // CID (16 bytes)
        memcpy(data, cid, 16);
        data += 16;
        
        // Framing Layer (77 bytes)
        // Framing Flags & Length (big-endian)
        uint16_t framingLength = FRAMING_LAYER_SIZE - 2 + DMP_LAYER_HEADER_SIZE + dataLength;
        *data++ = static_cast<uint8_t>((framingLength >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(framingLength & 0xFF);
        
        // Framing Vector (big-endian)
        *data++ = static_cast<uint8_t>((framingVector >> 24) & 0xFF);
        *data++ = static_cast<uint8_t>((framingVector >> 16) & 0xFF);
        *data++ = static_cast<uint8_t>((framingVector >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(framingVector & 0xFF);
        
        // Source Name (64 bytes)
        memcpy(data, sourceName, 64);
        data += 64;
        
        // Priority
        *data++ = priority;
        
        // Reserved
        *data++ = static_cast<uint8_t>((reserved >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(reserved & 0xFF);
        
        // Sequence Number
        *data++ = sequenceNumber;
        
        // Options
        *data++ = options;
        
        // Universe (big-endian)
        *data++ = static_cast<uint8_t>((universe >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(universe & 0xFF);
        
        // DMP Layer (11 + data bytes)
        // DMP Flags & Length (big-endian)
        uint16_t dmpLength = DMP_LAYER_HEADER_SIZE - 2 + dataLength;
        *data++ = static_cast<uint8_t>((dmpLength >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(dmpLength & 0xFF);
        
        // DMP Vector
        *data++ = dmpVector;
        
        // Address & Data Type
        *data++ = addressDataType;
        
        // First Property Address (big-endian)
        *data++ = static_cast<uint8_t>((firstPropertyAddress >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(firstPropertyAddress & 0xFF);
        
        // Address Increment (big-endian)
        *data++ = static_cast<uint8_t>((addressIncrement >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(addressIncrement & 0xFF);
        
        // Property Value Count (big-endian: 1 + data length)
        uint16_t valueCount = 1 + dataLength;
        *data++ = static_cast<uint8_t>((valueCount >> 8) & 0xFF);
        *data++ = static_cast<uint8_t>(valueCount & 0xFF);
        
        // DMX Start Code
        *data++ = dmxStartCode;
        
        // DMX Data
        memcpy(data, dmxData, dataLength);
    }
};

// E1.31 (sACN) sender class
class E131Sender : public DMXSender
{
public:
    E131Sender()
    {
        socket.bindToPort(0); // Bind to any available port
        
        // Generate unique CID (Component Identifier) for this sender instance
        // Use JUCE's Random to generate 16 random bytes
        juce::Random random;
        random.setSeedRandomly();
        for (int i = 0; i < 16; i++)
        {
            cid[i] = static_cast<uint8_t>(random.nextInt(256));
        }
        
        // Set source name
        juce::String sourceNameStr = "KeyGlow";
        memcpy(sourceName, sourceNameStr.toRawUTF8(), juce::jmin(64, sourceNameStr.length()));
    }
    
    ~E131Sender()
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
            
            E131Packet packet;
            
            // Set CID (Component Identifier) - unique per sender instance
            memcpy(packet.cid, cid, 16);
            
            // Set source name
            memcpy(packet.sourceName, sourceName, 64);
            
            // Set universe
            packet.universe = static_cast<uint16_t>(universeOffset);
            
            // Increment and wrap sequence number (0-255)
            sequenceNumber = (sequenceNumber + 1) % 256;
            packet.sequenceNumber = sequenceNumber;
            
            // Copy DMX data
            memcpy(packet.dmxData, dmxData + offset, channelsInThisPacket);
            
            // Convert to byte array
            juce::MemoryBlock block;
            packet.toByteArray(block, channelsInThisPacket);
            
            // Determine destination address
            juce::String destinationIP = targetIP;
            
            // E1.31 Multicast vs Unicast:
            // - Standard E1.31 multicast: 239.255.0.x where x = universe number
            // - For multicast, the IP should match the universe (e.g., universe 1 = 239.255.0.1)
            // - WLED typically prefers UNICAST (direct device IP) over multicast
            // - Multicast may not work due to router/network configuration (IGMP Snooping required)
            // - JUCE's DatagramSocket should handle multicast, but network/router must support it
            
            // If user entered a multicast address (239.x.x.x), use it as-is
            // If user entered unicast (192.168.x.x), use that (RECOMMENDED for WLED)
            // Note: The universe number is in the packet header, independent of IP address
            
            // For proper E1.31 multicast, the address should be 239.255.0.{universe}
            // But we allow any IP to be flexible (user can use unicast for WLED)
            
            // Send to E1.31 port (5568)
            socket.write(destinationIP, 5568, block.getData(), static_cast<int>(block.getSize()));
            
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
    uint8_t sequenceNumber = 0;
    uint8_t cid[16] = {0}; // Component Identifier (unique per sender instance)
    char sourceName[64] = {0}; // Source Name
};

