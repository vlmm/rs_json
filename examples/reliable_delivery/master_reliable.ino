/**
 * RS_JSON Reliable Delivery — Master (Client) Example
 *
 * Demonstrates the at-least-once delivery with deduplication protocol:
 *   1. Master periodically calls requestData() to ask the slave for data.
 *   2. The library automatically validates the checksum, deduplicates by ID,
 *      delivers the payload to the callback, and sends an ACK to the slave.
 *
 * Wiring (RS-485 half-duplex with DE/RE pin):
 *   Serial1 TX → RS-485 DI
 *   Serial1 RX → RS-485 RO
 *   Pin 4      → RS-485 DE/RE (high = transmit, low = receive)
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"

// RS_JSON master on Serial1, device address "master_01", DE/RE on pin 4
RS_JSON master(RS_JSON::MASTER, Serial1, "master_01", 4);

// Called by the library when a new (non-duplicate) "data" message arrives.
// The raw JSON string (without the trailing checksum) is passed in.
void onMessage(const char* jsonStr) {
    StaticJsonDocument<300> doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        Serial.println("JSON parse error in callback");
        return;
    }

    String command = doc["command"].as<String>();

    if (command == "data") {
        uint32_t id  = doc["id"].as<uint32_t>();
        JsonObject d = doc["data"];

        Serial.print("Received data ID=");
        Serial.print(id);
        Serial.print("  sensor=");
        Serial.println(d["sensor"].as<float>(), 2);
        // ACK is sent automatically by the library; no action needed here.

    } else if (command == "pong") {
        Serial.println("Received PONG");

    } else if (command == "discover_response") {
        Serial.print("Discovered: ");
        serializeJson(doc["data"], Serial);
        Serial.println();
    }
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);
    master.begin();
    master.setCallback(onMessage);
    Serial.println("MASTER ready. Requesting data every 2 s.");
}

void loop() {
    master.listen();  // process incoming bytes

    static uint32_t lastRequest = 0;
    if (millis() - lastRequest >= 2000) {
        lastRequest = millis();
        master.requestData("slave_01");  // triggers reliable request/data/ack flow
    }
}
