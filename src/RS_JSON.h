#ifndef RS_JSON_H
#define RS_JSON_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

/**
 * RS_JSON — RS485 JSON protocol library (MASTER / SLAVE FSM).
 *
 * ── Wire format ───────────────────────────────────────────────────────────────
 * Every line on the bus is:  <JSON><2-hex-checksum>\n
 * Checksum = (sum of all UTF-8 bytes of the JSON part) % 256, uppercase hex.
 *
 * ── Message schema ────────────────────────────────────────────────────────────
 *  Request  (MASTER → SLAVE):
 *    {"dst":"<slave>","src":"<master>","type":"req","id":<N>,"command":"<cmd>","data":{…}}
 *
 *  Response (SLAVE → MASTER):
 *    {"dst":"<master>","src":"<slave>","type":"resp","id":<N>,"command":"<cmd>","data":{…}}
 *
 *  ACK      (MASTER → SLAVE):
 *    {"dst":"<slave>","src":"<master>","type":"ack","id":<N>}
 *
 * ── ID rules ──────────────────────────────────────────────────────────────────
 *  • MASTER increments an internal counter and embeds the new ID in every request.
 *  • SLAVE echoes the request ID unchanged in its response.
 *  • Both sides use the ID for de-duplication (see FSM comments in .cpp).
 */
class RS_JSON {
public:
    enum Mode { MASTER, SLAVE };

    /** Called with the raw JSON part when a new (non-duplicate) request/response arrives. */
    using CallbackType = std::function<void(const char*)>;

    /** SLAVE only: called when MASTER sends an ACK confirming delivery of the last response. */
    using SuccessCallbackType = std::function<void()>;

    RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress);
    RS_JSON(Mode mode, HardwareSerial& serialPort, const String& deviceAddress, uint8_t dePin);

    /** Initialises serial communication. */
    void begin();
    void flush();

    /**
     * MASTER: send a request to a slave device.
     * A new auto-incremented ID is assigned internally.
     */
    void sendRequest(const String& dst, const String& command, const JsonObject& data);

    /**
     * SLAVE: send a response to the currently-processed request.
     * Must be called from within the request callback.
     * The library reuses the request ID and stores the response for re-sending on duplicates.
     */
    void sendResponse(const String& command, const JsonObject& data);

    /**
     * Process available serial bytes according to the FSM for the configured mode.
     * Call this from Arduino loop().
     */
    void listen();

    /** Convenience: send a "ping" request (MASTER) or respond to a ping (SLAVE via callback). */
    void ping(const String& address);

    /** Convenience: broadcast a "discover" request. */
    void discoverDevices();

    /** Register the data callback (request for SLAVE, response for MASTER). */
    void setCallback(CallbackType callback);

    /** SLAVE only: register the delivery-confirmed callback. */
    void setSuccessCallback(SuccessCallbackType callback);

    /**
     * MASTER only: set response-wait timeout in milliseconds (default 1000 ms).
     * After this period the pending request is abandoned and the next poll may proceed.
     */
    void setRequestTimeout(unsigned long ms);

private:
    Mode           mode_;
    HardwareSerial& serial_;
    String         address_;
    uint8_t        dePin_;
    bool           useDe_;
    String         buffer_;

    CallbackType        callback_;
    SuccessCallbackType successCallback_;

    // ── SLAVE state ───────────────────────────────────────────────────────────
    uint32_t lastRequestId_;      // ID of the last processed request (0 = none)
    uint32_t lastSentId_;         // ID embedded in the last sent response
    String   lastResponse_;       // Full wire bytes of the last response (for re-send)
    uint32_t currentRequestId_;   // ID of the request currently being processed
    String   currentRequestSrc_;  // Source address of the request being processed

    // ── MASTER state ──────────────────────────────────────────────────────────
    uint32_t     requestIdCounter_;          // Monotonic counter for request IDs
    String       pendingDevice_;             // Slave we are currently waiting on
    unsigned long requestSentAt_;            // millis() when the request was sent
    unsigned long requestTimeout_;           // Timeout in ms

    // Per-device last received response ID.
    // Fixed-size array avoids heap fragmentation on memory-constrained MCUs.
    static const uint8_t MAX_TRACKED_DEVICES = 8;
    struct DeviceRecord {
        char     address[32];
        uint32_t lastId;
        bool     valid;
    };
    DeviceRecord lastReceivedIds_[MAX_TRACKED_DEVICES];

    // ── Helpers ───────────────────────────────────────────────────────────────
    String calculateChecksum(const String& message);
    void   sendRaw(const String& jsonBody);
    void   sendAck(const String& dst, uint32_t id);
    void   processMessage(const String& message);
    void   startTransmission();
    void   endTransmission();
};

#endif // RS_JSON_H
