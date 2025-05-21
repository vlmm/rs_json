#include <Arduino.h>
#include <ArduinoJson.h>
#include "RS_JSON.h"
#include <SoftwareSerial.h>

// Create a software serial port if hardware is not used
SoftwareSerial softSerial(10, 11); // RX, TX

// Initialize RS_JSON as a slave device
RS_JSON device(RS_JSON::SLAVE, softSerial, 9600);

// Callback function to handle incoming messages
void handleMessage(const char* jsonMessage) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonMessage);

    if (error) {
        Serial.println("Invalid JSON");
        return;
    }

    String command = doc["command"];
    String fromAddress = doc["address"];
    JsonObject data = doc["data"];

    if (command == "ping") {
        // Respond to ping with pong
        StaticJsonDocument<50> responseData;
        responseData["response"] = "pong";
        device.sendMessage(fromAddress, "pong", responseData.as<JsonObject>());
    } 
    else if (command == "discover") {
        // Respond to discover with device information
        StaticJsonDocument<100> responseData;
        responseData["device_id"] = "slave_01";
        responseData["type"] = "sensor";
        responseData["status"] = "ready";
        device.sendMessage(fromAddress, "discover_response", responseData.as<JsonObject>());
    } 
    else {
        Serial.print("Unknown command: ");
        Serial.println(command);
    }
}

void setup() {
    Serial.begin(9600);         // For debugging
    device.begin();             // Start communication
    device.setCallback(handleMessage); // Register the callback function
    Serial.println("SLAVE device ready.");
}

void loop() {
    device.listen(); // Listen for incoming messages
}
