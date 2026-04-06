"""
RS_JSON Python helper — implements the same wire protocol as the C++ library.

Wire format
-----------
Every line on the bus:  <JSON><2-hex-checksum>\\n
Checksum = (sum of all UTF-8 bytes of the JSON part) % 256, uppercase hex.

Message schema
--------------
Request  (MASTER → SLAVE):
  {"dst":"<slave>","src":"<master>","type":"req","id":<N>,"command":"<cmd>","data":{…}}

Response (SLAVE → MASTER):
  {"dst":"<master>","src":"<slave>","type":"resp","id":<N>,"command":"<cmd>","data":{…}}

ACK      (MASTER → SLAVE):
  {"dst":"<slave>","src":"<master>","type":"ack","id":<N>}

ID rules
--------
- MASTER increments an internal counter and embeds the new ID in every request.
- SLAVE echoes the request ID unchanged in its response.
- MASTER de-duplicates responses per source device using the response ID.
- SLAVE de-duplicates requests using the request ID and resends the last response.
"""

import json
import serial
import serial.tools.list_ports
import time
from datetime import datetime


class RSJSON:
    BROADCAST = "broadcast"

    def __init__(self, port, baudrate, address="master", mode="master", timeout=1.0):
        """
        Parameters
        ----------
        port     : serial port name (e.g. '/dev/ttyUSB0' or 'COM3')
        baudrate : baud rate
        address  : this device's address on the bus (default "master")
        mode     : "master" or "slave"
        timeout  : MASTER response-wait timeout in seconds (default 1.0)
        """
        self.serial = serial.Serial(port, baudrate, timeout=1)
        self.serial.flush()
        self.address = address
        self.mode = mode.lower()
        self.request_timeout = timeout

        self.callback = None          # called with parsed dict on new request/response
        self.success_callback = None  # SLAVE only: called when ACK confirms delivery

        # ── MASTER state ──────────────────────────────────────────────────────
        self._request_id_counter = 0
        self._pending_device = None
        self._request_sent_at = None
        self._last_received_ids = {}  # {src_address: last_response_id}

        # ── SLAVE state ───────────────────────────────────────────────────────
        self._last_request_id = None      # ID of the last processed request
        self._last_sent_id = None         # ID used in the last sent response
        self._last_response = None        # Full wire string of the last response
        self._current_request_id = None   # ID of the request being processed
        self._current_request_src = None  # Source of the request being processed

    # ── Callbacks ─────────────────────────────────────────────────────────────

    def set_callback(self, callback):
        """Register callback invoked with the parsed message dict on new data."""
        self.callback = callback

    def set_success_callback(self, callback):
        """SLAVE only: register callback invoked when MASTER ACKs the last response."""
        self.success_callback = callback

    # ── Checksum ──────────────────────────────────────────────────────────────

    def calculate_checksum(self, message):
        """Return (sum of UTF-8 bytes) % 256."""
        return sum(message.encode('utf-8')) % 256

    # ── MASTER API ────────────────────────────────────────────────────────────

    def send_request(self, dst, command, data):
        """
        MASTER: send a request to a slave device and wait (blocking) for a response.
        Returns the parsed response dict, or None on timeout.
        """
        self._request_id_counter += 1
        msg = {
            "dst": dst,
            "src": self.address,
            "type": "req",
            "id": self._request_id_counter,
            "command": command,
            "data": data,
        }
        msg_str = json.dumps(msg, separators=(',', ':'))
        checksum = self.calculate_checksum(msg_str)
        full = f"{msg_str}{checksum:02X}\n"
        self.serial.write(full.encode('utf-8'))
        self._pending_device = dst
        self._request_sent_at = time.time()
        return self._wait_for_response()

    # ── SLAVE API ─────────────────────────────────────────────────────────────

    def send_response(self, command, data):
        """
        SLAVE: send a response to the currently-processed request.
        Call this from within the request callback.
        """
        msg = {
            "dst": self._current_request_src,
            "src": self.address,
            "type": "resp",
            "id": self._current_request_id,
            "command": command,
            "data": data,
        }
        msg_str = json.dumps(msg, separators=(',', ':'))
        checksum = self.calculate_checksum(msg_str)
        full = f"{msg_str}{checksum:02X}\n"
        self._last_sent_id = self._current_request_id
        self._last_response = full  # store for re-send on duplicate request
        self.serial.write(full.encode('utf-8'))

    # ── Shared / non-blocking listen ──────────────────────────────────────────

    def listen(self):
        """
        Non-blocking: process one pending line from serial.
        Returns the parsed message dict, or None if nothing new.
        """
        if self.serial.in_waiting > 0:
            line = self.serial.readline().decode('utf-8', errors='replace').strip()
            if line:
                return self._process_message(line)
        return None

    # ── Convenience helpers ───────────────────────────────────────────────────

    def ping(self, address):
        return self.send_request(address, "ping", {})

    def discover_devices(self):
        return self.send_request(self.BROADCAST, "discover", {})

    # ── Internal ──────────────────────────────────────────────────────────────

    def _wait_for_response(self):
        """Blocking wait for a valid response within request_timeout."""
        deadline = time.time() + self.request_timeout
        while time.time() < deadline:
            if self.serial.in_waiting > 0:
                line = self.serial.readline().decode('utf-8', errors='replace').strip()
                if line:
                    result = self._process_message(line)
                    if result is not None:
                        return result
            time.sleep(0.005)
        self._pending_device = None
        return None

    def _process_message(self, message):
        if len(message) < 3:
            return None

        json_part = message[:-2]
        checksum_str = message[-2:]

        try:
            expected = int(checksum_str, 16)
        except ValueError:
            print(f"RS_JSON: invalid checksum format in '{message}'")
            return None

        if self.calculate_checksum(json_part) != expected:
            print(f"RS_JSON: checksum mismatch in '{message}'")
            return None

        try:
            parsed = json.loads(json_part)
        except json.JSONDecodeError:
            print(f"RS_JSON: JSON decode error in '{message}'")
            return None

        msg_type = parsed.get("type", "")
        msg_id   = parsed.get("id")
        dst      = parsed.get("dst", "")
        src      = parsed.get("src", "")

        # ── SLAVE FSM ─────────────────────────────────────────────────────────
        if self.mode == "slave":
            if msg_type == "ack":
                # ACK: confirm that MASTER received our last response
                if dst == self.address and msg_id == self._last_sent_id:
                    if self.success_callback:
                        self.success_callback()
                return None

            # Only handle requests addressed to this device or broadcast
            if dst != self.address and dst != self.BROADCAST:
                return None

            if self._last_request_id is not None and msg_id == self._last_request_id:
                # Duplicate request — resend stored response without invoking callback
                if self._last_response:
                    self.serial.write(self._last_response.encode('utf-8'))
                return None

            # New request: store context for send_response(), invoke callback
            self._last_request_id = msg_id
            self._current_request_id = msg_id
            self._current_request_src = src
            if self.callback:
                self.callback(parsed)
            return parsed

        # ── MASTER FSM ────────────────────────────────────────────────────────
        if self.mode == "master":
            if msg_type != "resp":
                return None

            # Validate the response comes from the device we polled
            if self._pending_device and src != self._pending_device:
                return None

            is_dup = (self._last_received_ids.get(src) == msg_id)

            if not is_dup:
                self._last_received_ids[src] = msg_id
                if self.callback:
                    self.callback(parsed)

            self._send_ack(src, msg_id)
            self._pending_device = None
            return parsed if not is_dup else None

        return None

    def _send_ack(self, dst, msg_id):
        msg = {
            "dst": dst,
            "src": self.address,
            "type": "ack",
            "id": msg_id,
        }
        msg_str = json.dumps(msg, separators=(',', ':'))
        checksum = self.calculate_checksum(msg_str)
        full = f"{msg_str}{checksum:02X}\n"
        self.serial.write(full.encode('utf-8'))


# Example usage
if __name__ == "__main__":
    def handle_message(data):
        print(f"Callback received data: {data}")

    rs_json = None

    try:
        ports = list(serial.tools.list_ports.comports())
        if ports:
            # Use the first available port; adjust the index if a specific port is needed.
            print(f"Using serial port: {ports[0].device}")
            rs_json = RSJSON(ports[0].device, 9600, address="master", mode="master")
        else:
            raise Exception("No serial ports found.")

        rs_json.set_callback(handle_message)
        rs_json.discover_devices()
        rs_json.send_request("1", "ping", {})
        rs_json.send_request("1", "reset", {})
        rs_json.send_request("1", "enable", {})
        rs_json.send_request("1", "lcd", {
            "1": datetime.now().strftime("%H:%M:%S").center(16),
            "2": "".center(16),
        })

        last_read_command_time = time.time()
        read_command_interval = 1.0
        try:
            counter = 0
            while True:
                current_time = time.time()
                if current_time - last_read_command_time >= read_command_interval:
                    counter += 1
                    rs_json.send_request("1", "read", {})
                    last_read_command_time = current_time
                time.sleep(0.01)
        except KeyboardInterrupt:
            rs_json.send_request("1", "disable", {})
            print("Program terminated by user.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        if rs_json and rs_json.serial.is_open:
            print("Closing serial port.")
            rs_json.serial.flush()
            rs_json.serial.close()
