"""
rs_json.py — Python implementation of the RS_JSON protocol.

Wire format (matches the Arduino RS_JSON library):
    <JSON><CHECKSUM>\n
  where <CHECKSUM> is two uppercase hex characters equal to
  (sum of UTF-8 bytes of the JSON string) % 256.

Every JSON object contains at least:
    "address"  — destination device address (or "broadcast")
    "from"     — sender device address
    "command"  — command string
    "data"     — payload object (may be empty {})

Reliable delivery commands (at-least-once, with deduplication):
    "request"  Master → Slave  — asks for current data
    "data"     Slave  → Master — {"id": <uint>, "data": {...}}
    "ack"      Master → Slave  — {"id": <uint>}

Usage — Master:
    rs = RSJSON("master_pc", port="/dev/ttyUSB0", baudrate=9600,
                mode=RSJSON.MASTER)
    rs.set_callback(lambda msg: print("received:", msg))
    rs.request_data("slave_01")     # triggers request/data/ack flow
    rs.listen()                     # call in a loop; non-blocking

Usage — Slave (simulation / testing):
    rs = RSJSON("slave_01", port="/dev/ttyUSB0", baudrate=9600,
                mode=RSJSON.SLAVE)
    rs.set_data_provider(lambda: {"sensor": 3.14, "uptime": 42})
    rs.listen()                     # call in a loop; non-blocking
"""

import json
import time
import serial
import serial.tools.list_ports


class RSJSON:
    MASTER = "master"
    SLAVE  = "slave"

    def __init__(self, address: str, port: str, baudrate: int = 9600,
                 mode: str = "master", timeout: float = 0.01):
        """
        Parameters
        ----------
        address  : logical address of this device (e.g. "master_pc")
        port     : serial port (e.g. "/dev/ttyUSB0" or "COM3")
        baudrate : baud rate (default 9600)
        mode     : RSJSON.MASTER or RSJSON.SLAVE
        timeout  : serial read timeout in seconds (default 0.01 s — non-blocking)
        """
        self.address  = address
        self.mode     = mode
        self.serial   = serial.Serial(port, baudrate, timeout=timeout)
        self._buffer  = b""

        # Callbacks
        self._callback      = None   # called with the parsed message dict
        self._data_provider = None   # SLAVE: callable() → dict with current data
        self._last_sent_id    = 0
        self._last_acked_id   = 0
        self._last_sent_payload = b""   # wire bytes to retransmit

        # MASTER deduplication state
        self._last_processed_id = 0

    # ------------------------------------------------------------------ #
    # Configuration                                                        #
    # ------------------------------------------------------------------ #

    def set_callback(self, callback):
        """Register a function called with the parsed message dict when a
        new (non-duplicate) message is received."""
        self._callback = callback

    def set_data_provider(self, provider):
        """SLAVE only: register a callable that returns a dict with the
        current data to send in response to a "request" command."""
        self._data_provider = provider

    # ------------------------------------------------------------------ #
    # Checksum                                                             #
    # ------------------------------------------------------------------ #

    @staticmethod
    def calculate_checksum(message_str: str) -> str:
        """Return two uppercase hex characters (sum of UTF-8 bytes % 256)."""
        total = sum(message_str.encode("utf-8")) % 256
        return f"{total:02X}"

    # ------------------------------------------------------------------ #
    # Low-level send                                                       #
    # ------------------------------------------------------------------ #

    def _transmit(self, wire_bytes: bytes):
        """Write raw wire bytes to the serial port."""
        self.serial.write(wire_bytes)

    def _build_wire(self, address: str, command: str, data: dict,
                    extra: dict = None) -> bytes:
        """
        Serialise a message to wire format: <JSON><CHECKSUM>\\n

        extra  — additional top-level JSON fields (e.g. {"id": 5})
        """
        msg = {
            "address": address,
            "from":    self.address,
            "command": command,
            "data":    data or {},
        }
        if extra:
            msg.update(extra)
        json_str  = json.dumps(msg, separators=(",", ":"))
        checksum  = self.calculate_checksum(json_str)
        return (json_str + checksum + "\n").encode("utf-8")

    # ------------------------------------------------------------------ #
    # Public send methods                                                  #
    # ------------------------------------------------------------------ #

    def send_message(self, address: str, command: str, data: dict = None):
        """Send an arbitrary command. Compatible with legacy usage."""
        wire = self._build_wire(address, command, data or {})
        self._transmit(wire)

    def request_data(self, slave_address: str):
        """MASTER: send a reliable data request to *slave_address*."""
        wire = self._build_wire(slave_address, "request", {})
        self._transmit(wire)

    def send_ack(self, slave_address: str, msg_id: int):
        """MASTER: acknowledge delivery of *msg_id* to *slave_address*.
        Called automatically by listen(); exposed for manual use."""
        wire = self._build_wire(slave_address, "ack", {"id": msg_id})
        self._transmit(wire)

    def ping(self, address: str):
        """Send a ping to *address*."""
        self.send_message(address, "ping", {"timestamp": int(time.time() * 1000)})

    def discover_devices(self):
        """Broadcast a discovery request to all devices on the bus."""
        self.send_message("broadcast", "discover", {"request": "who_is_there"})

    # ------------------------------------------------------------------ #
    # Receive / process                                                    #
    # ------------------------------------------------------------------ #

    def listen(self):
        """
        Non-blocking read: drain all available bytes from the serial port,
        process any complete lines, and return the last successfully parsed
        message dict (or None if nothing was received).
        """
        result = None
        raw = self.serial.read(self.serial.in_waiting or 1)
        if raw:
            self._buffer += raw
            while b"\n" in self._buffer:
                line, self._buffer = self._buffer.split(b"\n", 1)
                msg = self._process_message(line.rstrip(b"\r").decode("utf-8", errors="replace"))
                if msg is not None:
                    result = msg
        return result

    def _process_message(self, message: str):
        """
        Validate checksum and dispatch the message according to the
        current mode.  Returns the parsed dict on success, None otherwise.
        """
        if len(message) < 3:
            return None

        json_part     = message[:-2]
        checksum_part = message[-2:].upper()

        if self.calculate_checksum(json_part) != checksum_part:
            return None

        try:
            msg = json.loads(json_part)
        except json.JSONDecodeError:
            return None

        recv_address = msg.get("address", "")
        sender       = msg.get("from", "")
        command      = msg.get("command", "")
        data         = msg.get("data", {})

        # ── SLAVE mode ───────────────────────────────────────────────────
        if self.mode == self.SLAVE:
            if recv_address not in (self.address, "broadcast"):
                return None

            if command == "request":
                self._handle_request(sender)
                return None

            if command == "ack":
                acked_id = data.get("id") if isinstance(data, dict) else None
                if acked_id is not None and acked_id == self._last_sent_id:
                    self._last_acked_id = acked_id
                return None

            # All other commands → user callback
            if self._callback:
                self._callback(msg)
            return msg

        # ── MASTER mode ──────────────────────────────────────────────────
        if self.mode == self.MASTER:
            if command == "data":
                if recv_address != self.address:
                    return None
                msg_id = msg.get("id", 0)
                if msg_id != self._last_processed_id:
                    self._last_processed_id = msg_id
                    if self._callback:
                        self._callback(msg)
                # Always ACK (even duplicates)
                if sender:
                    self.send_ack(sender, msg_id)
                return msg

            if command in ("ack", "request"):
                return None

            # Other commands (pong, discover_response, …) → user callback
            if self._callback and recv_address != self.address:
                self._callback(msg)
            return msg

        return None

    def _handle_request(self, requester_address: str):
        """SLAVE: respond to a "request" command with data or a retransmit."""
        if self._last_sent_id == self._last_acked_id:
            # Previous data was ACKed (or this is the first request) → new data
            self._last_sent_id += 1
            current_data = self._data_provider() if self._data_provider else {}
            wire = self._build_wire(
                requester_address, "data", current_data,
                extra={"id": self._last_sent_id}
            )
            self._last_sent_payload = wire
            self._transmit(wire)
        else:
            # Previous data not yet ACKed → retransmit same payload
            self._transmit(self._last_sent_payload)

    # ------------------------------------------------------------------ #
    # Resource management                                                  #
    # ------------------------------------------------------------------ #

    def close(self):
        """Flush and close the serial port."""
        if self.serial.is_open:
            self.serial.flush()
            self.serial.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


# ────────────────────────────────────────────────────────────────────────── #
# Example usage                                                               #
# ────────────────────────────────────────────────────────────────────────── #

if __name__ == "__main__":
    import sys

    def on_message(msg):
        command = msg.get("command")
        sender  = msg.get("from", "?")
        data    = msg.get("data", {})

        if command == "data":
            print(f"[DATA] id={msg.get('id')}  from={sender}  payload={data}")
        elif command == "pong":
            print(f"[PONG] from={sender}")
        elif command == "discover_response":
            print(f"[DISCOVER] from={sender}  info={data}")
        else:
            print(f"[MSG] command={command}  from={sender}  data={data}")

    # Auto-detect first available serial port
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("No serial ports found.", file=sys.stderr)
        sys.exit(1)

    port_name = ports[0].device
    print(f"Using serial port: {port_name}")

    with RSJSON("master_pc", port=port_name, baudrate=9600,
                mode=RSJSON.MASTER) as rs:
        rs.set_callback(on_message)

        # Basic discovery and ping
        rs.discover_devices()
        rs.ping("slave_01")

        # Reliable data polling: request every second
        last_request = 0.0
        try:
            while True:
                rs.listen()
                now = time.time()
                if now - last_request >= 1.0:
                    last_request = now
                    rs.request_data("slave_01")
                time.sleep(0.01)
        except KeyboardInterrupt:
            print("\nTerminated by user.")

