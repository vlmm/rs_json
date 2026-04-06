#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"

// Initialize RS_JSON as master with address "master"
RS_JSON master(RS_JSON::MASTER, Serial1, "master");

/**
 * Called when a new (non-duplicate) response arrives from a slave.
 * Receiving the callback means MASTER has already sent an ACK automatically.
 */
void handleResponse(const char* jsonMessage) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, jsonMessage) != DeserializationError::Ok) {
        Serial.println("Received invalid JSON");
        return;
    }

    const char* command = doc["command"] | "";

    if (strcmp(command, "pong") == 0) {
        Serial.println("Received PONG response from device.");
    } else if (strcmp(command, "discover_response") == 0) {
        Serial.println("Discovered device:");
        serializeJsonPretty(doc["data"], Serial);
        Serial.println();
    } else {
        Serial.print("Received unknown command: ");
        Serial.println(command);
    }
}

// Simple poll scheduler: track last-poll time per device
static unsigned long lastPollTime = 0;
static const unsigned long POLL_INTERVAL = 2000; // poll every 2 s

void setup() {
    Serial.begin(9600);    // debug port
    Serial1.begin(9600);   // RS485 port
    master.begin();
    master.setCallback(handleResponse);
    master.setRequestTimeout(500);  // 500 ms response timeout
    Serial.println("MASTER device ready.");
}

void loop() {
    // Poll slave_01 periodically; listen() handles the response and sends ACK.
    unsigned long now = millis();
    if (now - lastPollTime >= POLL_INTERVAL) {
        lastPollTime = now;
        master.ping("slave_01");
    }
    master.listen();
}
