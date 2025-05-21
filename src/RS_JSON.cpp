#include "RS_JSON.h"


RS_JSON::RS_JSON(Mode mode, Stream& serialPort, const String& deviceAddress)
    : mode(mode), serial(serialPort), address(deviceAddress) {
}

void RS_JSON::begin() {
}

String RS_JSON::calculateChecksum(const String& message) {
    int sum = 0;
    for (char c : message) {
        sum += c;
    }
    // Изчисляваме сумата по модул 256
    int checksum = sum % 256;
    // Форматираме като шестнадесетичен низ с два символа
    String hexChecksum = String(checksum, HEX);
    // Уверяваме се, че е с два символа, добавяйки нули отпред, ако е необходимо
    if (hexChecksum.length() < 2) {
        hexChecksum = "0" + hexChecksum;
    }
    hexChecksum.toUpperCase(); // Преобразуваме в главни букви
    return hexChecksum; // Връщаме шестнадесетичния низ
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

    serial.println(message);
}

void RS_JSON::listen() {
    if (serial.available()) {
        String message = serial.readStringUntil('\n');
        processMessage(message);
    }
}

void RS_JSON::processMessage(const String& message) {
    // Assume last 2 characters are the checksum
    String jsonPart = message.substring(0, message.length() - 2);
    String checksumPart = message.substring(message.length() - 2);
#ifdef DEBUG
    Serial.println(message);
#endif
    // Validate checksum
    if (calculateChecksum(jsonPart) != checksumPart) {
        // Invalid checksum, discard message
        Serial.println("Invalid checksum");
        return;
    }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonPart);

    if (!error) {
        String receivedAddress = doc["address"]; // Използваме "receivedAddress" за да не объркаме с член-променливата "address"
        String command = doc["command"];
        JsonObject data = doc["data"];

        // Handle command here (extend logic as needed)

        // Call the registered callback with the raw JSON part
        // ONLY if the received address matches our device's address or "broadcast"
        if (callback_ && (receivedAddress == this->address || receivedAddress == "broadcast")) {
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
