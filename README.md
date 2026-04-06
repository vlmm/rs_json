# RS JSON Library

## Description (English)
The "RS JSON" library provides an interface for RS485 communication supporting both Master and Slave devices. It uses JSON message format and implements a reliable request/response protocol with message IDs, ACKs, and de-duplication.

## Описание (Български)
Библиотеката "RS JSON" предоставя интерфейс за работа с RS485 комуникация, поддържащ Master и Slave устройства. Използва JSON формат за съобщения с надеждна логика: идентификатори на съобщения, потвърждения (ACK) и защита срещу дублирани съобщения.

---

## Message Schema

Every line transmitted on the bus has the form:

```
<JSON><2-hex-checksum>\n
```

The **checksum** is the sum of all UTF-8 bytes of the JSON part, modulo 256, formatted as two uppercase hex digits (e.g. `A3`).

### Request (MASTER → SLAVE)

```json
{"dst":"slave_01","src":"master","type":"req","id":1,"command":"ping","data":{}}
```

### Response (SLAVE → MASTER)

```json
{"dst":"master","src":"slave_01","type":"resp","id":1,"command":"pong","data":{"response":"pong"}}
```

### ACK (MASTER → SLAVE)

```json
{"dst":"slave_01","src":"master","type":"ack","id":1}
```

### Field reference

| Field     | Type            | Description                                      |
|-----------|-----------------|--------------------------------------------------|
| `dst`     | string          | Destination device address (or `"broadcast"`)    |
| `src`     | string          | Source device address                            |
| `type`    | `"req"` / `"resp"` / `"ack"` | Message type                      |
| `id`      | uint32          | Message ID (see rules below)                     |
| `command` | string          | Application-level command name                   |
| `data`    | object          | Application-level payload (omitted in ACK)       |

### ID rules

- **MASTER** increments a monotonic counter and embeds the new `id` in every request.  
  The diagram's *"NO ID"* node means the scheduler does not impose an application-level ID — the library assigns one transparently.
- **SLAVE** echoes the request `id` unchanged in its response.
- **MASTER** uses the response `id` for **per-device de-duplication**: if the same `id` arrives again from the same slave, it sends an ACK but does not re-invoke the callback.
- **SLAVE** uses the request `id` for **duplicate detection**: if the same `id` arrives again, it re-sends the stored last response without re-invoking the callback.

---

## Project Structure

```
/RS_JSON
│
├── /src
│   ├── RS_JSON.h
│   └── RS_JSON.cpp
│
├── /python
│   └── rs_json.py
│
└── /examples
    ├── /master
    │   └── master.ino
    └── /slave
        └── slave.ino
```

---

## C++ API Quick Reference

```cpp
// Construction
RS_JSON device(RS_JSON::SLAVE,  serial, "slave_01");
RS_JSON master(RS_JSON::MASTER, serial, "master");
RS_JSON master(RS_JSON::MASTER, serial, "master", DE_PIN); // RS485 DE pin

// Common
device.begin();
device.setCallback([](const char* json){ /* handle */ });

// SLAVE
device.setSuccessCallback([]{ /* ACK received */ });
// inside the callback:
device.sendResponse("pong", data.as<JsonObject>());

// MASTER
master.setRequestTimeout(500);   // ms, default 1000
master.sendRequest("slave_01", "read", data.as<JsonObject>());
master.ping("slave_01");
master.discoverDevices();

// Call from loop()
device.listen();
```

---

## Python API Quick Reference

```python
from rs_json import RSJSON

# MASTER
rs = RSJSON('/dev/ttyUSB0', 9600, address='master', mode='master')
rs.set_callback(lambda msg: print(msg))
rs.send_request('slave_01', 'read', {})   # blocking, returns response dict or None

# SLAVE
rs = RSJSON('/dev/ttyUSB0', 9600, address='slave_01', mode='slave')
rs.set_callback(lambda msg: rs.send_response('ok', {'value': 42}))
rs.set_success_callback(lambda: print('ACKed'))
while True:
    rs.listen()
```

---

## Requirements

- Arduino IDE  
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library (install via Library Manager)

## Python Requirements

```
pyserial
```
