#include "RS_JSON.h"

RS_JSON::RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress)
    : mode(mode), serial(serialPort), address(deviceAddress), useDe(false),
      lastSentId(0), lastAckedId(0), lastProcessedId(0) {
}

RS_JSON::RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress, uint8_t dePin)
    : mode(mode), serial(serialPort), address(deviceAddress), dePin(dePin), useDe(true),
      lastSentId(0), lastAckedId(0), lastProcessedId(0) {
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

void RS_JSON::rawTransmit(const String& msgWithChecksum) {
    if (useDe) {
        startTransmission();
    }
    serial.print(msgWithChecksum);
    serial.print('\n');
    if (useDe) {
        endTransmission();
    }
}

void RS_JSON::sendMessage(const String& address, const String& command, const JsonObject& data) {
    // 300 bytes accommodates address, from, command, id, and a typical data payload
    StaticJsonDocument<300> doc;
    doc["address"] = address;
    doc["from"]    = this->address;
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
    rawTransmit(message);
}

void RS_JSON::requestData(const String& address) {
    // No payload fields are needed for a request; pass an empty object.
    // 32 bytes is ample for an unused pool that holds zero JSON nodes.
    StaticJsonDocument<32> emptyData;
    sendMessage(address, "request", emptyData.as<JsonObject>());
}

void RS_JSON::sendAck(const String& address, uint32_t id) {
    StaticJsonDocument<50> dataDoc;
    dataDoc["id"] = id;
    sendMessage(address, "ack", dataDoc.as<JsonObject>());
}

void RS_JSON::setDataProvider(DataProviderType provider) {
    dataProvider_ = provider;
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
    if (message.length() < 3) return;  // minimum valid: 1 byte JSON + 2 bytes checksum

    // Assume last 2 characters are the checksum
    String jsonPart = message.substring(0, message.length() - 2);
    String checksumPart = message.substring(message.length() - 2);

#ifdef RS_JSON_DEBUG
    Serial.println(message);
#endif

    // Validate checksum
    if (calculateChecksum(jsonPart) != checksumPart) {
#ifdef RS_JSON_DEBUG
        Serial.print("Invalid checksum for: ");
        Serial.println(jsonPart);
#endif
        return;
    }

    // 300 bytes accommodates all fields including "from", "id", and user data
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, jsonPart);

    if (error) return;

    String receivedAddress = doc["address"].as<String>();
    String senderAddress   = doc["from"].as<String>();
    String command         = doc["command"].as<String>();
    JsonObject data        = doc["data"];

    // ── SLAVE mode ──────────────────────────────────────────────────────────
    if (mode == SLAVE) {
        if (receivedAddress != this->address && receivedAddress != "broadcast") return;

        if (command == "request") {
            if (lastSentId == lastAckedId) {
                // All previously sent data was acknowledged → send new data with a new ID
                lastSentId++;

                // 300 bytes: address, from, command, id, plus user-provided data payload
                StaticJsonDocument<300> respDoc;
                respDoc["address"] = senderAddress;
                respDoc["from"]    = this->address;
                respDoc["command"] = "data";
                respDoc["id"]      = lastSentId;

                JsonObject dataObj = respDoc.createNestedObject("data");
                if (dataProvider_) {
                    dataProvider_(dataObj);
                }

                String msg;
                serializeJson(respDoc, msg);
                msg += calculateChecksum(msg);
                lastSentPayload = msg;  // store for possible retransmit
                rawTransmit(msg);
            } else {
                // Unacked data exists → retransmit last payload unchanged
                rawTransmit(lastSentPayload);
            }
            return;
        }

        if (command == "ack") {
            if (!data.isNull()) {
                uint32_t ackedId = data["id"].as<uint32_t>();
                if (ackedId == lastSentId) {
                    lastAckedId = ackedId;
                }
            }
            return;
        }

        // All other commands → deliver to user callback
        if (callback_) {
            callback_(jsonPart.c_str());
        }
        return;
    }

    // ── MASTER mode ─────────────────────────────────────────────────────────
    if (mode == MASTER) {
        if (command == "data") {
            // "data" responses are addressed to the master
            if (receivedAddress != this->address) return;

            uint32_t id = doc["id"].as<uint32_t>();

            if (id != lastProcessedId) {
                // New ID: process the payload and record it
                lastProcessedId = id;
                if (callback_) {
                    callback_(jsonPart.c_str());
                }
            }
            // Always acknowledge, even for duplicates
            if (senderAddress.length() > 0) {
                sendAck(senderAddress, id);
            }
            return;
        }

        // Master never handles "ack" or "request" commands
        if (command == "ack" || command == "request") return;

        // Existing behavior for all other commands (ping, pong, discover, etc.)
        if (callback_ && receivedAddress != this->address) {
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