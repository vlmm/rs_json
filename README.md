# RS JSON Library

## Description (English)
The "RS JSON" library provides an interface for RS-485 communication supporting both Master and Slave devices. It uses a JSON message format with a trailing two-hex-character checksum and includes an **at-least-once reliable delivery** protocol with automatic retransmit and deduplication.

Available for:
- **Arduino / C++** — `src/RS_JSON.h` + `src/RS_JSON.cpp`
- **Python** — `python/rs_json.py` (wire-compatible with the Arduino library)

## Описание (Български)
Библиотеката "RS JSON" предоставя интерфейс за RS-485 комуникация, поддържаща Master и Slave устройства. Използва JSON формат с двусимволен hex контролен сбор и протокол за **надеждна доставка** (at-least-once) с автоматично преизпращане и дедупликация.

Налична за:
- **Arduino / C++** — `src/RS_JSON.h` + `src/RS_JSON.cpp`
- **Python** — `python/rs_json.py` (wire-съвместима с Arduino библиотеката)

---

## Project Structure

```
RS_JSON/
├── src/
│   ├── RS_JSON.h                              C++ library header
│   └── RS_JSON.cpp                            C++ library implementation
├── examples/
│   ├── master/
│   │   └── master.ino                         Basic ping/discover (Master)
│   ├── slave/
│   │   └── slave.ino                          Basic ping/discover (Slave)
│   └── reliable_delivery/
│       ├── master_reliable.ino                Reliable request/data/ack (Master)
│       └── slave_reliable.ino                 Reliable request/data/ack (Slave)
├── python/
│   ├── rs_json.py                             Python library
│   └── README.md                              Python library documentation
├── docs/
│   └── RS_JSON_Interface_Manual.md
├── keywords.txt
└── library.properties
```

---

## Requirements

### Arduino / C++
- Arduino IDE
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) library (install via Library Manager)

### Python
- Python ≥ 3.7
- [pyserial](https://pypi.org/project/pyserial/) (`pip install pyserial`)

---

## Installation

### Arduino
1. Create a directory named `RS_JSON` in your Arduino `libraries` folder.
2. Copy `RS_JSON.h` and `RS_JSON.cpp` into a `src` subdirectory.
3. Copy the `examples` folder alongside `src`.

### Python
```bash
pip install pyserial
# Copy python/rs_json.py into your project, then:
from rs_json import RSJSON
```

---

## Wire Protocol

Every message is a single newline-terminated line:

```
<JSON><CHECKSUM>\n
```

| Part | Description |
|------|-------------|
| `<JSON>` | JSON object with `"address"`, `"from"`, `"command"`, `"data"` |
| `<CHECKSUM>` | Two uppercase hex characters = (sum of UTF-8 bytes of JSON) % 256 |

### Reliable delivery commands

| Command | Direction | Key fields |
|---------|-----------|-----------|
| `"request"` | Master → Slave | `address`, `from` |
| `"data"` | Slave → Master | `address`, `from`, `id` (uint32), `data` |
| `"ack"` | Master → Slave | `address`, `from`, `data.id` (uint32) |

---

## C++ API

### Constructor

```cpp
RS_JSON(Mode mode, HardwareSerial& serial, const String& deviceAddress);
RS_JSON(Mode mode, HardwareSerial& serial, const String& deviceAddress, uint8_t dePin);
```

`dePin` — RS-485 Driver-Enable / Receiver-Enable pin (optional, for half-duplex).

### Common methods

```cpp
void begin();
void listen();                                               // call in loop()
void setCallback(std::function<void(const char*)> cb);      // raw JSON callback
void sendMessage(const String& addr, const String& cmd,
                 const JsonObject& data);                   // send arbitrary command
void ping(const String& address);
void discoverDevices();
```

### Reliable delivery — Master

```cpp
void requestData(const String& slaveAddress);   // send request; auto-ACK on receipt
void sendAck(const String& slaveAddress, uint32_t id);  // normally automatic
```

### Reliable delivery — Slave

```cpp
// Register a callback that fills the outgoing JsonObject with current data.
void setDataProvider(std::function<void(JsonObject&)> provider);
```

### Debug output

Define `RS_JSON_DEBUG` before including the header to enable `Serial` trace prints:

```cpp
#define RS_JSON_DEBUG
#include "RS_JSON.h"
```

---

## Python API

See [`python/README.md`](python/README.md) for the full Python API reference.

Quick example:

```python
from rs_json import RSJSON
import time

def on_message(msg):
    if msg["command"] == "data":
        print(f"id={msg['id']}  data={msg['data']}")

with RSJSON("master_pc", port="/dev/ttyUSB0", baudrate=9600,
            mode=RSJSON.MASTER) as rs:
    rs.set_callback(on_message)
    last = 0.0
    while True:
        rs.listen()
        if time.time() - last >= 1.0:
            last = time.time()
            rs.request_data("slave_01")
        time.sleep(0.01)
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

**Deduplication (Master):** if a `"data"` message arrives with an already-processed `id`, the user callback is **not** invoked again, but the ACK is still sent so the slave can advance.
