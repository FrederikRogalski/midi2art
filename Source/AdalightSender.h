/*
  ==============================================================================

    AdalightSender.h
    Created: 2025
    Author: KeyGlow Project

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DMXSender.h"

#if JUCE_MAC || JUCE_LINUX
    #include <fcntl.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
#elif JUCE_WINDOWS
    #include <windows.h>
#endif

// Adalight sender class - sends LED data over USB serial
class AdalightSender : public DMXSender
{
public:
    AdalightSender()
        #if JUCE_WINDOWS
        : serialHandle(INVALID_HANDLE_VALUE)
        #else
        : serialHandle(-1)
        #endif
    {
    }
    
    ~AdalightSender()
    {
        closeSerialPort();
    }
    
    // For Adalight, "targetIP" is actually the serial port name/path
    void setTargetIP(const juce::String& serialPortName) override
    {
        if (currentSerialPort != serialPortName)
        {
            currentSerialPort = serialPortName;
            openSerialPort();
        }
    }
    
    // Universe not used for Adalight
    void setUniverse(int universe) override
    {
        // Not applicable for serial protocol
    }
    
    void sendDMX(const uint8_t* dmxData, int numChannels) override
    {
        if (!isConnected() || numChannels == 0)
            return;
        
        // Calculate number of LEDs (3 bytes per LED)
        int numLEDs = numChannels / 3;
        
        // Build complete packet: 6 byte header + RGB data
        // Use pre-allocated buffer to avoid heap allocation on audio thread
        const int packetSize = 6 + numChannels;
        if (packetSize > MAX_PACKET_SIZE)
            return;
        
        // Header
        packetBuffer[0] = 'A';
        packetBuffer[1] = 'd';
        packetBuffer[2] = 'a';
        
        // LED count - 1 (as per Adalight protocol)
        uint16_t ledCountMinusOne = static_cast<uint16_t>(numLEDs - 1);
        uint8_t hi = static_cast<uint8_t>((ledCountMinusOne >> 8) & 0xFF);
        uint8_t lo = static_cast<uint8_t>(ledCountMinusOne & 0xFF);
        
        packetBuffer[3] = hi;
        packetBuffer[4] = lo;
        packetBuffer[5] = hi ^ lo ^ 0x55;  // Checksum
        
        // Copy RGB data after header
        memcpy(packetBuffer + 6, dmxData, numChannels);
        
        // Send entire packet
        writeSerial(packetBuffer, packetSize);
    }
    
    // Get list of available serial ports
    static juce::StringArray getAvailableSerialPorts()
    {
        juce::StringArray ports;
        
        #if JUCE_MAC || JUCE_LINUX
            // On macOS/Linux, scan /dev for serial ports
            juce::File devDir("/dev");
            
            if (devDir.exists())
            {
                // Look for common serial port patterns
                for (const auto& entry : juce::RangedDirectoryIterator(devDir, false))
                {
                    juce::String name = entry.getFile().getFileName();
                    
                    #if JUCE_MAC
                        // macOS: look for cu.* devices (callout devices are preferred over tty.*)
                        if (name.startsWith("cu."))
                        {
                            ports.add(entry.getFile().getFullPathName());
                        }
                    #elif JUCE_LINUX
                        // Linux: look for ttyUSB*, ttyACM* (common USB serial adapters)
                        if (name.startsWith("ttyUSB") || name.startsWith("ttyACM"))
                        {
                            ports.add(entry.getFile().getFullPathName());
                        }
                    #endif
                }
            }
        #elif JUCE_WINDOWS
            // On Windows, enumerate COM ports by trying to open them
            for (int i = 1; i <= 20; i++)
            {
                juce::String portName = "\\\\.\\COM" + juce::String(i);
                HANDLE handle = CreateFileA(portName.toRawUTF8(),
                                           GENERIC_READ | GENERIC_WRITE,
                                           0, NULL, OPEN_EXISTING, 0, NULL);
                if (handle != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(handle);
                    ports.add("COM" + juce::String(i));
                }
            }
        #endif
        
        return ports;
    }
    
    // Check if serial port is open and working
    bool isConnected() const
    {
        #if JUCE_WINDOWS
            return serialHandle != INVALID_HANDLE_VALUE;
        #else
            return serialHandle >= 0;
        #endif
    }
    
private:
    #if JUCE_WINDOWS
        HANDLE serialHandle;
    #else
        int serialHandle;
    #endif
    
    juce::String currentSerialPort;
    
    // Pre-allocated packet buffer: 6-byte header + max 512 LEDs * 3 bytes = 1542 bytes
    static constexpr int MAX_PACKET_SIZE = 6 + 512 * 3;
    uint8_t packetBuffer[MAX_PACKET_SIZE] = {0};
    
    void openSerialPort()
    {
        closeSerialPort();
        
        if (currentSerialPort.isEmpty())
            return;
        
        #if JUCE_WINDOWS
            // Windows implementation
            juce::String portPath = "\\\\.\\" + currentSerialPort;
            serialHandle = CreateFileA(portPath.toRawUTF8(),
                                      GENERIC_READ | GENERIC_WRITE,
                                      0,
                                      NULL,
                                      OPEN_EXISTING,
                                      0,
                                      NULL);
            
            if (serialHandle == INVALID_HANDLE_VALUE)
                return;
            
            // Configure serial port
            DCB dcbSerialParams = {0};
            dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
            
            if (!GetCommState(serialHandle, &dcbSerialParams))
            {
                CloseHandle(serialHandle);
                serialHandle = INVALID_HANDLE_VALUE;
                return;
            }
            
            dcbSerialParams.BaudRate = CBR_115200;
            dcbSerialParams.ByteSize = 8;
            dcbSerialParams.StopBits = ONESTOPBIT;
            dcbSerialParams.Parity = NOPARITY;
            
            if (!SetCommState(serialHandle, &dcbSerialParams))
            {
                CloseHandle(serialHandle);
                serialHandle = INVALID_HANDLE_VALUE;
                return;
            }
            
            // Set timeouts
            COMMTIMEOUTS timeouts = {0};
            timeouts.ReadIntervalTimeout = 50;
            timeouts.ReadTotalTimeoutConstant = 50;
            timeouts.ReadTotalTimeoutMultiplier = 10;
            timeouts.WriteTotalTimeoutConstant = 50;
            timeouts.WriteTotalTimeoutMultiplier = 10;
            
            SetCommTimeouts(serialHandle, &timeouts);
            
        #else
            // POSIX implementation (macOS/Linux)
            serialHandle = open(currentSerialPort.toRawUTF8(), O_RDWR | O_NOCTTY | O_NDELAY);
            
            if (serialHandle < 0)
                return;
            
            // Configure serial port
            struct termios options;
            tcgetattr(serialHandle, &options);
            
            // Set baud rate
            cfsetispeed(&options, B115200);
            cfsetospeed(&options, B115200);
            
            // 8N1 mode (8 data bits, no parity, 1 stop bit)
            options.c_cflag &= ~PARENB;  // No parity
            options.c_cflag &= ~CSTOPB;  // 1 stop bit
            options.c_cflag &= ~CSIZE;
            options.c_cflag |= CS8;      // 8 data bits
            
            // Raw mode
            options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
            options.c_oflag &= ~OPOST;
            options.c_iflag &= ~(IXON | IXOFF | IXANY);
            
            // Enable receiver, ignore modem control lines
            options.c_cflag |= (CLOCAL | CREAD);
            
            // Set timeouts
            options.c_cc[VMIN] = 0;
            options.c_cc[VTIME] = 10;
            
            // Apply settings
            if (tcsetattr(serialHandle, TCSANOW, &options) != 0)
            {
                close(serialHandle);
                serialHandle = -1;
                return;
            }
            
            // Flush any existing data
            tcflush(serialHandle, TCIOFLUSH);
        #endif
    }
    
    void closeSerialPort()
    {
        if (isConnected())
        {
            #if JUCE_WINDOWS
                CloseHandle(serialHandle);
                serialHandle = INVALID_HANDLE_VALUE;
            #else
                close(serialHandle);
                serialHandle = -1;
            #endif
        }
    }
    
    void writeSerial(const uint8_t* data, size_t length)
    {
        if (!isConnected())
            return;
        
        #if JUCE_WINDOWS
            DWORD bytesWritten;
            WriteFile(serialHandle, data, static_cast<DWORD>(length), &bytesWritten, NULL);
        #else
            write(serialHandle, data, length);
        #endif
    }
};
