# RS JSON Library

## Description (English)
The "RS JSON" library provides an interface for RS-485 communication supporting both Master and Slave devices. It uses a JSON message format with a trailing two-hex-character checksum. Version 2 adds an **at-least-once reliable delivery** protocol with deduplication.

## Описание (Български)
Библиотеката "RS JSON" предоставя интерфейс за RS-485 комуникация, поддържаща Master и Slave устройства. Използва JSON формат с двусимволен hex контролен сбор. Версия 2 добавя протокол за **надеждна доставка** (at-least-once) с дедупликация.

---

## Project Structure

```
/RS_JSON
│
├── /src
│   ├── RS_JSON.h
│   └── RS_JSON.cpp
│
└── /examples
    ├── /master
    │   └── master.ino            (basic ping/discover example)
    ├── /slave
    │   └── slave.ino             (basic ping/discover example)
    └── /reliable_delivery
        ├── master_reliable.ino   (request/data/ack Master example)
        └── slave_reliable.ino    (request/data/ack Slave example)
```

---

## Requirements (English)
- Arduino IDE
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library (install via Library Manager)

## Изисквания (Български)
- Arduino IDE
- Библиотека [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (инсталирайте чрез Library Manager)

---

## Installation (English)
1. Create a directory named `RS_JSON` in the `libraries` folder of your Arduino installation.
2. Copy `RS_JSON.h` and `RS_JSON.cpp` into a `src` subdirectory.
3. Copy the `examples` folder alongside `src`.

## Инсталация (Български)
1. Създайте директория `RS_JSON` в папката `libraries` на Arduino инсталацията.
2. Копирайте `RS_JSON.h` и `RS_JSON.cpp` в поддиректория `src`.
3. Копирайте папката `examples` до `src`.

---

## Wire Protocol

Every message is a single line:

```
<JSON>{CHECKSUM}\n
```

- `<JSON>` — a JSON object with at least the fields `"address"`, `"from"`, `"command"`, and `"data"`.
- `{CHECKSUM}` — two uppercase hex characters (sum of all JSON bytes mod 256).

### Reliable delivery commands

| Command | Sender | Key fields |
|---------|--------|-----------|
| `"request"` | Master → Slave | `address`, `from` |
| `"data"` | Slave → Master | `address`, `from`, `id` (uint32), `data` |
| `"ack"` | Master → Slave | `address`, `from`, `data.id` (uint32) |

---

## API

### Constructor

```cpp
RS_JSON(Mode mode, HardwareSerial& serial, const String& deviceAddress);
RS_JSON(Mode mode, HardwareSerial& serial, const String& deviceAddress, uint8_t dePin);
```

`dePin` is the RS-485 Driver-Enable/Receiver-Enable pin (optional).

### Common methods

```cpp
void begin();
void listen();                                                // call in loop()
void setCallback(std::function<void(const char*)> cb);       // raw JSON callback
void sendMessage(const String& addr, const String& cmd,
                 const JsonObject& data);                    // send arbitrary command
void ping(const String& address);
void discoverDevices();
```

### Reliable delivery — Master

```cpp
// Send a data request to the slave. The library handles checksum, dedup, and auto-ACK.
void requestData(const String& slaveAddress);

// Manually send an ACK (normally called automatically on "data" reception).
void sendAck(const String& slaveAddress, uint32_t id);
```

### Reliable delivery — Slave

```cpp
// Register a callback that fills the outgoing JsonObject with current sensor data.
void setDataProvider(std::function<void(JsonObject&)> provider);
```

---

## Reliable Delivery Flow

```
Master                              Slave
  |                                   |
  |--- request ---------------------->|
  |                                   | (first request, or last ID was ACKed)
  |                                   |  → call dataProvider(), assign new ID
  |<-- data (id=N, payload, CRC) -----|
  |                                   |
  | (CRC OK, id not yet processed)    |
  | → invoke callback                 |
  | → record id as processed          |
  |--- ack (id=N) ----------------->  |
  |                                   | → mark ID as delivered
  |                                   |
  |--- request ---------------------->|  (next cycle, id N was ACKed)
  |                                   |  → call dataProvider(), assign id=N+1
  |<-- data (id=N+1, ...) -----------|
  ...

If ACK never arrives:
  |--- request ---------------------->|
  |                                   | (id N still unACKed)
  |<-- data (id=N, same payload) -----|  retransmit
  |--- ack (id=N) ----------------->  |
```

### Deduplication (Master)
If the master receives a `"data"` message with an `id` it has already processed (`id == lastProcessedId`), it **does not** invoke the user callback again but **does** resend the ACK so the slave can advance.

### Debug output
Define `RS_JSON_DEBUG` before including the header to enable `Serial` trace prints:

```cpp
#define RS_JSON_DEBUG
#include "RS_JSON.h"
```
