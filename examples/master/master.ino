#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"
#include <SoftwareSerial.h>

SoftwareSerial softSerial(10, 11); // RX, TX

// Initialize RS_JSON as the master device
RS_JSON master(RS_JSON::MASTER, softSerial, 9600);

// Callback function for incoming messages
void handleMessage(const char* jsonMessage) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonMessage);

    if (error) {
        Serial.println("Received invalid JSON");
        return;
    }

    String command = doc["command"];
    JsonObject data = doc["data"];

    if (command == "pong") {
        Serial.println("Received PONG response from device.");
    } else if (command == "discover_response") {
        Serial.println("Discovered device:");
        serializeJsonPretty(data, Serial);
        Serial.println();
    } else {
        Serial.print("Received unknown command: ");
        Serial.println(command);
    }
}

void setup() {
    Serial.begin(9600);             // For debugging
    master.begin();                 // Start communication
    master.setCallback(handleMessage); // Register the callback function

    Serial.println("MASTER device ready.");

    // Send a ping to a specific device
    master.ping("slave_01");

    // Send a broadcast discover message
    master.discoverDevices();
}

void loop() {
    master.listen(); // Listen for responses from devices
}
