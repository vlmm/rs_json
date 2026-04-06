#include "RS_JSON.h"

// ── Construction ──────────────────────────────────────────────────────────────

RS_JSON::RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress)
    : mode_(mode), serial_(serialPort), address_(deviceAddress),
      dePin_(0), useDe_(false),
      lastRequestId_(0), lastSentId_(0), currentRequestId_(0),
      requestIdCounter_(0), requestSentAt_(0), requestTimeout_(1000)
{}

RS_JSON::RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress, uint8_t dePin)
    : mode_(mode), serial_(serialPort), address_(deviceAddress),
      dePin_(dePin), useDe_(true),
      lastRequestId_(0), lastSentId_(0), currentRequestId_(0),
      requestIdCounter_(0), requestSentAt_(0), requestTimeout_(1000)
{}

// ── Public API ────────────────────────────────────────────────────────────────

void RS_JSON::begin() {}

void RS_JSON::flush() { serial_.flush(); }

void RS_JSON::setCallback(CallbackType cb)             { callback_ = cb; }
void RS_JSON::setSuccessCallback(SuccessCallbackType cb) { successCallback_ = cb; }
void RS_JSON::setRequestTimeout(unsigned long ms)      { requestTimeout_ = ms; }

void RS_JSON::sendRequest(const String& dst, const String& command, const JsonObject& data) {
    ++requestIdCounter_;

    StaticJsonDocument<256> doc;
    doc["dst"]     = dst;
    doc["src"]     = address_;
    doc["type"]    = "req";
    doc["id"]      = requestIdCounter_;
    doc["command"] = command;
    JsonObject dataObj = doc.createNestedObject("data");
    for (JsonPair kv : data) {
        dataObj[kv.key()] = kv.value();
    }

    String jsonBody;
    serializeJson(doc, jsonBody);

    pendingDevice_ = dst;
    requestSentAt_ = millis();
    sendRaw(jsonBody);
}

void RS_JSON::sendResponse(const String& command, const JsonObject& data) {
    StaticJsonDocument<256> doc;
    doc["dst"]     = currentRequestSrc_;
    doc["src"]     = address_;
    doc["type"]    = "resp";
    doc["id"]      = currentRequestId_;
    doc["command"] = command;
    JsonObject dataObj = doc.createNestedObject("data");
    for (JsonPair kv : data) {
        dataObj[kv.key()] = kv.value();
    }

    String jsonBody;
    serializeJson(doc, jsonBody);

    String checksum = calculateChecksum(jsonBody);
    String full     = jsonBody + checksum + '\n';

    lastSentId_   = currentRequestId_;
    lastResponse_ = full;  // store for potential re-send on duplicate request

    if (useDe_) startTransmission();
    serial_.print(full);
    if (useDe_) endTransmission();
}

void RS_JSON::listen() {
    // MASTER: abandon pending request after timeout so next poll may proceed
    if (mode_ == MASTER && pendingDevice_.length() > 0) {
        if (millis() - requestSentAt_ > requestTimeout_) {
            pendingDevice_ = "";
        }
    }

    while (serial_.available()) {
        char c = serial_.read();
        if (c != '\n' && c != '\r') {
            buffer_ += c;
        } else if (buffer_.length() > 0) {
            processMessage(buffer_);
            buffer_ = "";
        }
    }
}

void RS_JSON::ping(const String& dst) {
    StaticJsonDocument<16> empty;
    sendRequest(dst, "ping", empty.as<JsonObject>());
}

void RS_JSON::discoverDevices() {
    StaticJsonDocument<16> empty;
    sendRequest("broadcast", "discover", empty.as<JsonObject>());
}

// ── Private helpers ───────────────────────────────────────────────────────────

String RS_JSON::calculateChecksum(const String& message) {
    int sum = 0;
    for (char c : message) sum += (uint8_t)c;
    int checksum = sum % 256;
    String hex = String(checksum, HEX);
    if (hex.length() < 2) hex = "0" + hex;
    hex.toUpperCase();
    return hex;
}

void RS_JSON::sendRaw(const String& jsonBody) {
    String checksum = calculateChecksum(jsonBody);
    String full     = jsonBody + checksum + '\n';
    if (useDe_) startTransmission();
    serial_.print(full);
    if (useDe_) endTransmission();
}

void RS_JSON::sendAck(const String& dst, uint32_t id) {
    StaticJsonDocument<128> doc;
    doc["dst"]  = dst;
    doc["src"]  = address_;
    doc["type"] = "ack";
    doc["id"]   = id;
    String jsonBody;
    serializeJson(doc, jsonBody);
    sendRaw(jsonBody);
}

void RS_JSON::processMessage(const String& message) {
    if (message.length() < 3) return;

    String jsonPart    = message.substring(0, message.length() - 2);
    String checksumStr = message.substring(message.length() - 2);
    checksumStr.toUpperCase();

    if (calculateChecksum(jsonPart) != checksumStr) {
        Serial.print("RS_JSON: bad checksum: ");
        Serial.println(message);
        return;
    }

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, jsonPart) != DeserializationError::Ok) return;

    const char* type = doc["type"] | "";
    uint32_t    id   = doc["id"]   | 0;
    const char* dst  = doc["dst"]  | "";
    const char* src  = doc["src"]  | "";

    // ── SLAVE FSM ─────────────────────────────────────────────────────────────
    if (mode_ == SLAVE) {
        if (strcmp(type, "ack") == 0) {
            // ACK: confirm that MASTER received our last response
            if (strcmp(dst, address_.c_str()) == 0 && id == lastSentId_) {
                if (successCallback_) successCallback_();
            }
            return;
        }

        // Only handle requests addressed to this device or broadcast
        if (strcmp(dst, address_.c_str()) != 0 && strcmp(dst, "broadcast") != 0) return;

        if (lastRequestId_ != 0 && id == lastRequestId_) {
            // Duplicate request — resend the stored response without invoking callback
            if (lastResponse_.length() > 0) {
                if (useDe_) startTransmission();
                serial_.print(lastResponse_);
                if (useDe_) endTransmission();
            }
            return;
        }

        // New request: store ID, set context for sendResponse(), invoke callback
        lastRequestId_     = id;
        currentRequestId_  = id;
        currentRequestSrc_ = String(src);

        if (callback_) callback_(jsonPart.c_str());
        // The user must call sendResponse() from within the callback.
        return;
    }

    // ── MASTER FSM ────────────────────────────────────────────────────────────
    if (mode_ == MASTER) {
        if (strcmp(type, "resp") != 0) return;  // MASTER only processes responses

        // Validate the response comes from the device we polled
        if (pendingDevice_.length() > 0 && strcmp(src, pendingDevice_.c_str()) != 0) return;

        String srcStr = String(src);
        auto   it     = lastReceivedIds_.find(srcStr);
        bool   isDup  = (it != lastReceivedIds_.end() && it->second == id);

        if (!isDup) {
            lastReceivedIds_[srcStr] = id;
            if (callback_) callback_(jsonPart.c_str());
        }

        sendAck(srcStr, id);
        pendingDevice_ = "";  // return to idle
    }
}

void RS_JSON::startTransmission() {
    serial_.flush();
    digitalWrite(dePin_, HIGH);
    delay(2);
}

void RS_JSON::endTransmission() {
    serial_.flush();
    delay(2);
    digitalWrite(dePin_, LOW);
}