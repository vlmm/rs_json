#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"

// Initialize RS_JSON as a slave device with address "slave_01"
RS_JSON device(RS_JSON::SLAVE, Serial1, "slave_01");

/**
 * Called when a new (non-duplicate) request arrives.
 * Use device.sendResponse() to reply — the library records the response
 * for automatic re-sending if the same request ID is received again.
 */
void handleRequest(const char* jsonMessage) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, jsonMessage) != DeserializationError::Ok) {
        Serial.println("Invalid JSON");
        return;
    }

    const char* command = doc["command"] | "";

    if (strcmp(command, "ping") == 0) {
        StaticJsonDocument<64> responseData;
        responseData["response"] = "pong";
        device.sendResponse("pong", responseData.as<JsonObject>());

    } else if (strcmp(command, "discover") == 0) {
        StaticJsonDocument<128> responseData;
        responseData["device_id"] = "slave_01";
        responseData["type"]      = "sensor";
        responseData["status"]    = "ready";
        device.sendResponse("discover_response", responseData.as<JsonObject>());

    } else {
        Serial.print("Unknown command: ");
        Serial.println(command);
    }
}

/** Called when MASTER sends an ACK confirming our last response was delivered. */
void handleDelivered() {
    Serial.println("Last response was ACKed by master.");
}

void setup() {
    Serial.begin(9600);   // debug port
    Serial1.begin(9600);  // RS485 port
    device.begin();
    device.setCallback(handleRequest);
    device.setSuccessCallback(handleDelivered);
    Serial.println("SLAVE device ready.");
}

void loop() {
    device.listen();
}
