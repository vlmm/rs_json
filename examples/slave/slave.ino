/**
 * RS_JSON — Slave (Server) Basic Example
 *
 * Handles ping and discover commands from a Master over RS-485.
 * Uses HardwareSerial (Serial1) for RS-485 communication and
 * Serial (USB) for debug output.
 *
 * Wiring (RS-485 half-duplex with DE/RE pin):
 *   Serial1 TX → RS-485 DI
 *   Serial1 RX → RS-485 RO
 *   Pin 4      → RS-485 DE/RE (high = transmit, low = receive)
 *
 * For reliable request/data/ack, see examples/reliable_delivery/slave_reliable.ino
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"

// RS_JSON slave on Serial1, device address "slave_01", DE/RE on pin 4
RS_JSON device(RS_JSON::SLAVE, Serial1, "slave_01", 4);

// Called by the library for every message addressed to this device
// that passes checksum validation (excluding "request" and "ack", which
// are handled internally by the reliable-delivery logic).
void handleMessage(const char* jsonStr) {
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.println("JSON parse error in callback");
        return;
    }

    String command  = doc["command"].as<String>();
    String fromAddr = doc["from"].as<String>();  // reply address
    JsonObject data = doc["data"];

    if (command == "ping") {
        // Reply with pong
        StaticJsonDocument<50> resp;
        resp["ts"] = millis();
        device.sendMessage(fromAddr, "pong", resp.as<JsonObject>());

    } else if (command == "discover") {
        // Reply with device info
        StaticJsonDocument<100> resp;
        resp["device_id"] = "slave_01";
        resp["type"]      = "sensor";
        resp["status"]    = "ready";
        device.sendMessage(fromAddr, "discover_response", resp.as<JsonObject>());

    } else {
        Serial.print("Unknown command: ");
        Serial.println(command);
    }
}

void setup() {
    Serial.begin(115200);   // USB debug output
    Serial1.begin(9600);    // RS-485 bus
    device.begin();
    device.setCallback(handleMessage);
    Serial.println("SLAVE ready.");
}

void loop() {
    device.listen();  // process any incoming bytes
}
