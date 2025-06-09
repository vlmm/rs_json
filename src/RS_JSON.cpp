#include "RS_JSON.h"

RS_JSON::RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress)
    : mode(mode), serial(serialPort), address(deviceAddress), useDe(false) {
}

RS_JSON::RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress, uint8_t dePin)
    : mode(mode), serial(serialPort), address(deviceAddress), dePin(dePin), useDe(true) {
}

void RS_JSON::begin() {
    // Initialization code can be added here if needed
}

void RS_JSON::flush() {
    serial.flush();
}

String RS_JSON::calculateChecksum(const String& message) {
    int sum = 0;
    for (char c : message) {
        sum += c;
    }
    // Calculate the sum modulo 256
    int checksum = sum % 256;
    // Format as a two-character hexadecimal string
    String hexChecksum = String(checksum, HEX);
    // Ensure it is two characters long by padding with zeros if necessary
    if (hexChecksum.length() < 2) {
        hexChecksum = "0" + hexChecksum;
    }
    hexChecksum.toUpperCase(); // Convert to uppercase
    return hexChecksum; // Return the hexadecimal string
}

void RS_JSON::sendMessage(const String& address, const String& command, const JsonObject& data) {
    StaticJsonDocument<200> doc;
    doc["address"] = address;
    doc["command"] = command;

    // Copy all key-value pairs from the input data to a nested "data" object
    JsonObject dataObj = doc.createNestedObject("data");
    for (JsonPair kv : data) {
        dataObj[kv.key()] = kv.value();
    }

    String message;
    serializeJson(doc, message);
    String checksum = calculateChecksum(message);
    message += checksum; // Append checksum after JSON content
    message += '\n';

    if (useDe) {
        startTransmission();
    }
    serial.print(message);
    if (useDe) {
        endTransmission();
    }
}

void RS_JSON::listen() {
    while (serial.available()) {
        char c = serial.read();

        // Accumulate characters in the buffer
        if ((c != '\n') && (c != '\r')) {
            buffer += c;
        } else {
            // End of message
            processMessage(buffer);
            buffer = "";  // Clear the buffer
        }
    }
}

void RS_JSON::processMessage(const String& message) {
    // Assume last 2 characters are the checksum
    String jsonPart = message.substring(0, message.length() - 2);
    String checksumPart = message.substring(message.length() - 2);
//#ifdef DEBUG
    Serial.println(message);
//#endif
    // Validate checksum
    if (calculateChecksum(jsonPart) != checksumPart) {
        // Invalid checksum, discard message
        Serial.print("Invalid checksum '");
        Serial.println(jsonPart);
        Serial.print("' | '");
        Serial.print(checksumPart);
        Serial.println("'");
        return;
    }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonPart);

    if (!error) {
        String receivedAddress = doc["address"]; // Use "receivedAddress" to avoid confusion with member variable "address"
        String command = doc["command"];
        JsonObject data = doc["data"];

        // Handle command here (extend logic as needed)

        // Call the registered callback with the raw JSON part
        // ONLY if the received address matches our device's address or "broadcast"
            // MASTER не трябва да обработва съобщения адресирани до себе си
        if (mode == MASTER) {
            // В MASTER режим: callback се вика само ако съобщението НЕ е адресирано до нас
            if (callback_ && receivedAddress != this->address) {
                callback_(jsonPart.c_str());
            }
        } else if (callback_ && (receivedAddress == this->address || receivedAddress == "broadcast")) {
            callback_(jsonPart.c_str());
        }
    }
}

void RS_JSON::ping(const String& address) {
    StaticJsonDocument<50> data;
    data["timestamp"] = millis(); // Optional: can help measure latency
    sendMessage(address, "ping", data.as<JsonObject>());
}

void RS_JSON::discoverDevices() {
    StaticJsonDocument<50> data;
    data["request"] = "who_is_there";
    sendMessage("broadcast", "discover", data.as<JsonObject>());
}

void RS_JSON::setCallback(CallbackType callback) {
    callback_ = callback;
}

void RS_JSON::startTransmission() {
    serial.flush();
    digitalWrite(dePin, HIGH);
    delay(2);
}

void RS_JSON::endTransmission() {
    serial.flush();
    delay(2);
    digitalWrite(dePin, LOW);
}