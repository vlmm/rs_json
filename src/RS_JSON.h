#ifndef RS_JSON_H
#define RS_JSON_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>


class RS_JSON {
public:
    enum Mode { MASTER, SLAVE };

    // Type definition for the message-received callback
    using CallbackType = std::function<void(const char*)>;

    // Type definition for the SLAVE data-provider callback
    using DataProviderType = std::function<void(JsonObject&)>;

    // Constructor: Initializes the communication mode, serial port, and device address
    RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress);

    RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress, uint8_t dePin);

    // Initializes serial communication
    void begin();
    void flush();

    // Sends a JSON-formatted message to a specified address with a command and data
    void sendMessage(const String& address, const String& command, const JsonObject& data);

    // MASTER: sends a data request to the slave at the given address
    void requestData(const String& address);

    // MASTER: sends an acknowledgement for a received data ID to the slave
    void sendAck(const String& address, uint32_t id);

    // SLAVE: registers a callback that populates the outgoing JsonObject with current data
    void setDataProvider(DataProviderType provider);

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
    HardwareSerial& serial;       // Reference to the serial port
    String address;               // Device's own address
    CallbackType callback_;       // Registered callback function
    DataProviderType dataProvider_; // SLAVE data provider callback
    String buffer;
    uint8_t dePin;
    bool useDe;

    // SLAVE reliability state
    uint32_t lastSentId;          // ID of the last data message sent
    uint32_t lastAckedId;         // ID of the last data message acknowledged
    String   lastSentPayload;     // Wire payload (JSON+checksum, no newline) for retransmit

    // MASTER deduplication state
    uint32_t lastProcessedId;     // ID of the last "data" message processed

    // Calculates a simple checksum for the given message
    String calculateChecksum(const String& message);

    // Parses and processes a received message
    void processMessage(const String& message);

    // Sends msgWithChecksum followed by a newline, with optional DE control
    void rawTransmit(const String& msgWithChecksum);

    void startTransmission();
    void endTransmission();
};

#endif // RS_JSON_H
