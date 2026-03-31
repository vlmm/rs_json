# RS_JSON — Python Library

Python implementation of the RS_JSON protocol.  
Mirrors the Arduino C++ library and is fully wire-compatible with it.

## Requirements

```
pip install pyserial
```

Python ≥ 3.7 required.

---

## Wire Protocol

Every message is a single newline-terminated line:

```
<JSON><CHECKSUM>\n
```

| Part | Description |
|------|-------------|
| `<JSON>` | JSON object with `"address"`, `"from"`, `"command"`, `"data"` |
| `<CHECKSUM>` | Two uppercase hex characters = (sum of UTF-8 bytes of the JSON string) % 256 |

### Reliable delivery commands

| Command | Direction | Key fields in `data` |
|---------|-----------|----------------------|
| `"request"` | Master → Slave | *(empty)* |
| `"data"` | Slave → Master | `id` (int), plus user fields |
| `"ack"` | Master → Slave | `id` (int) |

---

## Quick Start

### Master — request/data/ack (reliable delivery)

```python
from rs_json import RSJSON
import time

def on_message(msg):
    if msg["command"] == "data":
        print(f"id={msg['id']}  payload={msg['data']}")

with RSJSON("master_pc", port="/dev/ttyUSB0", baudrate=9600,
            mode=RSJSON.MASTER) as rs:
    rs.set_callback(on_message)

    last = 0.0
    while True:
        rs.listen()                          # non-blocking; call in every loop iteration
        if time.time() - last >= 1.0:
            last = time.time()
            rs.request_data("slave_01")      # reliable request → auto ACK on receipt
        time.sleep(0.01)
```

### Master — ping / discover

```python
from rs_json import RSJSON

def on_message(msg):
    print(msg)

with RSJSON("master_pc", port="/dev/ttyUSB0", baudrate=9600,
            mode=RSJSON.MASTER) as rs:
    rs.set_callback(on_message)
    rs.ping("slave_01")
    rs.discover_devices()

    import time
    deadline = time.time() + 2.0
    while time.time() < deadline:
        rs.listen()
        time.sleep(0.01)
```

### Slave simulation (Python ↔ Python testing)

```python
from rs_json import RSJSON
import time

sensor_value = 0.0

def provide_data():
    global sensor_value
    sensor_value += 0.1
    return {"sensor": round(sensor_value, 2), "uptime": int(time.time())}

def on_message(msg):
    # ping, discover, and other non-reliable commands arrive here
    print("Received:", msg)

with RSJSON("slave_01", port="/dev/ttyUSB1", baudrate=9600,
            mode=RSJSON.SLAVE) as rs:
    rs.set_data_provider(provide_data)
    rs.set_callback(on_message)

    while True:
        rs.listen()
        time.sleep(0.01)
```

---

## API Reference

### Constructor

```python
RSJSON(address, port, baudrate=9600, mode=RSJSON.MASTER, timeout=0.01)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `address` | str | Logical address of this device (e.g. `"master_pc"`) |
| `port` | str | Serial port (`"/dev/ttyUSB0"`, `"COM3"`, …) |
| `baudrate` | int | Baud rate (default `9600`) |
| `mode` | str | `RSJSON.MASTER` or `RSJSON.SLAVE` |
| `timeout` | float | Serial read timeout in seconds (default `0.01` — non-blocking) |

### Common methods

```python
rs.listen()                              # → last parsed msg dict or None
rs.set_callback(fn)                      # fn(msg: dict)
rs.send_message(address, command, data)  # send arbitrary command
rs.ping(address)
rs.discover_devices()
rs.close()                               # flush and close port
```

### Reliable delivery — Master

```python
rs.request_data(slave_address)   # send "request"; library auto-ACKs received "data"
rs.send_ack(slave_address, id)   # send "ack" manually (normally automatic)
```

### Reliable delivery — Slave

```python
rs.set_data_provider(fn)   # fn() → dict  called to fill data on each "request"
```

---

## Reliable Delivery Flow

```
Master                              Slave
  |                                   |
  |--- request ---------------------->|
  |                                   | (first request, or last ID was ACKed)
  |                                   |  → call data_provider(), assign new ID
  |<-- data (id=N, payload, CRC) -----|
  |                                   |
  | (CRC OK, id not yet processed)    |
  | → invoke callback                 |
  | → record id as processed          |
  |--- ack (id=N) ----------------->  |
  |                                   | → mark ID as delivered
  |                                   |
  |--- request ---------------------->|  (next cycle)
  ...

If ACK never arrives:
  |--- request ---------------------->|
  |                                   | (id N still unACKed)
  |<-- data (id=N, same payload) -----|  retransmit
  |--- ack (id=N) ----------------->  |
```

**Deduplication (Master):** if a `"data"` message arrives with an `id` already processed, the callback is **not** invoked again, but the ACK is still sent.

---

## Context Manager

`RSJSON` supports the `with` statement:

```python
with RSJSON("master_pc", port="/dev/ttyUSB0") as rs:
    rs.ping("slave_01")
# serial port closed automatically
```
