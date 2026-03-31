/**
 * RS_JSON — Master (Client) Basic Example
 *
 * Demonstrates ping and device discovery over RS-485.
 * Uses HardwareSerial (Serial1) for RS-485 communication and
 * Serial (USB) for debug output.
 *
 * Wiring (RS-485 half-duplex with DE/RE pin):
 *   Serial1 TX → RS-485 DI
 *   Serial1 RX → RS-485 RO
 *   Pin 4      → RS-485 DE/RE (high = transmit, low = receive)
 *
 * For reliable request/data/ack, see examples/reliable_delivery/master_reliable.ino
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"

// RS_JSON master on Serial1, device address "master_01", DE/RE on pin 4
RS_JSON master(RS_JSON::MASTER, Serial1, "master_01", 4);

// Called by the library for every message that passes checksum validation
// and is NOT addressed to this device (i.e. responses from slaves).
void handleMessage(const char* jsonStr) {
    StaticJsonDocument<300> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        Serial.println("JSON parse error in callback");
        return;
    }

    String command = doc["command"].as<String>();
    String from    = doc["from"].as<String>();
    JsonObject data = doc["data"];

    if (command == "pong") {
        Serial.print("PONG from ");
        Serial.println(from);

    } else if (command == "discover_response") {
        Serial.print("Discovered device from ");
        Serial.print(from);
        Serial.print(": ");
        serializeJson(data, Serial);
        Serial.println();

    } else {
        Serial.print("Unknown command '");
        Serial.print(command);
        Serial.print("' from ");
        Serial.println(from);
    }
}

void setup() {
    Serial.begin(115200);   // USB debug output
    Serial1.begin(9600);    // RS-485 bus
    master.begin();
    master.setCallback(handleMessage);

    Serial.println("MASTER ready.");

    // Send a ping to a specific slave
    master.ping("slave_01");

    // Broadcast a discovery request to all devices on the bus
    master.discoverDevices();
}

void loop() {
    master.listen();  // process any incoming bytes
}
