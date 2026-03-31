/**
 * RS_JSON Reliable Delivery — Slave (Server) Example
 *
 * Demonstrates the at-least-once delivery with deduplication protocol:
 *   1. On each "request" from the master the library calls the data provider
 *      to fill in the current sensor reading, assigns a new ID, and sends it.
 *   2. If the previous transmission was not yet acknowledged the library
 *      automatically retransmits the same payload (same ID) instead.
 *   3. When an "ack" arrives the ID is marked as delivered.
 *
 * Wiring (RS-485 half-duplex with DE/RE pin):
 *   Serial1 TX → RS-485 DI
 *   Serial1 RX → RS-485 RO
 *   Pin 4      → RS-485 DE/RE (high = transmit, low = receive)
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"

// RS_JSON slave on Serial1, device address "slave_01", DE/RE on pin 4
RS_JSON slave(RS_JSON::SLAVE, Serial1, "slave_01", 4);

// Called by the library to fill in the current data before sending a response.
// Place the latest sensor readings into `out`.
void provideData(JsonObject& out) {
    out["sensor"] = analogRead(A0) * (5.0f / 1023.0f);  // example: ADC voltage (assumes 5 V AREF)
    out["uptime"] = millis() / 1000UL;
}

// Optional: called for non-reliable commands (ping, discover, etc.)
void onMessage(const char* jsonStr) {
    StaticJsonDocument<300> doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) return;

    String command    = doc["command"].as<String>();
    String fromAddr   = doc["from"].as<String>();

    if (command == "ping") {
        StaticJsonDocument<50> resp;
        resp["ts"] = millis();
        slave.sendMessage(fromAddr, "pong", resp.as<JsonObject>());

    } else if (command == "discover") {
        StaticJsonDocument<100> resp;
        resp["device_id"] = "slave_01";
        resp["type"]      = "sensor";
        resp["status"]    = "ready";
        slave.sendMessage(fromAddr, "discover_response", resp.as<JsonObject>());
    }
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(9600);
    slave.begin();
    slave.setDataProvider(provideData);  // register data provider for request/data/ack flow
    slave.setCallback(onMessage);        // register callback for other commands
    Serial.println("SLAVE ready.");
}

void loop() {
    slave.listen();  // process incoming bytes (request, ack, ping, discover, ...)
}
