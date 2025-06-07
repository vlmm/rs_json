#ifndef RS_JSON_H
#define RS_JSON_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

class RS_JSON {
public:
    enum Mode { MASTER, SLAVE };

    // Type definition for the callback function
    using CallbackType = std::function<void(const char*)>;

    // Constructor: Initializes the communication mode, serial port, baud rate, and device address
    RS_JSON(Mode mode, Stream& serialPort, const String& deviceAddress);

    // Initializes serial communication
    void begin();

    // Sends a JSON-formatted message to a specified address with a command and data
    void sendMessage(const String& address, const String& command, const JsonObject& data);

    // Listens for incoming messages and processes them
    void listen();

    // Sends a ping message to the specified address
    void ping(const String& address);

    // Sends a discovery message to detect available devices
    void discoverDevices();

    // Registers a callback function to be called when a message is received
    void setCallback(CallbackType callback);

private:
    Mode mode;                    // Operating mode (MASTER or SLAVE)
    Stream& serial;               // Reference to the serial port
    int baudRate;                 // Communication baud rate
    String address;               // Device's own address
    CallbackType callback_;       // Registered callback function
    String buffer;

    // Calculates a simple checksum for the given message
    String calculateChecksum(const String& message);

    // Parses and processes a received message
    void processMessage(const String& message);
};

#endif // RS_JSON_H
