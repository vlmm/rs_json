import json
import serial
import serial.tools.list_ports
import time
from datetime import datetime


class RSJSON:
    def __init__(self, port, baudrate):
        self.serial = serial.Serial(port, baudrate, timeout=1)
        self.serial.flush()
        self.callback = None
        self.last_send_time = None

    def set_callback(self, callback):
        self.callback = callback

    def calculate_checksum(self, message):
        return sum(message.encode('utf-8')) % 256

    def send_message(self, address, command, data):
        message = {
            "address": address,
            "command": command,
            "data": data
        }
        message_str = json.dumps(message, separators=(',', ':'))
        checksum = self.calculate_checksum(message_str)
        full_message = f"{message_str}{checksum:02X}\n"
        self.serial.write(full_message.encode('utf-8'))
        self.last_send_time = time.time()
        self.listen()

    def listen(self, timeout=0.5):
        """Waits for a response for a specified timeout (blocking), returns data or None."""
        while (time.time() - self.last_send_time) < timeout:
            if self.serial.in_waiting > 0:
                line = self.serial.readline().decode('utf-8', errors='replace').strip()
                data = self.process_message(line)
                if data and self.callback:
                    self.callback(data)
                return data
            time.sleep(0.01)  # Avoid 100% CPU usage
        return None

    def process_message(self, message):
        print(f"Message received: '{message}'")

        if len(message) < 2:
            print(f"Invalid message format. '{message}'")
            return None

        json_part = message[:-2]  # Everything except the last two characters
        checksum_str = message[-2:]  # The last two characters

        try:
            expected_checksum = int(checksum_str, 16)  # Convert from hexadecimal
        except ValueError:
            print(f"Invalid checksum format. '{message}'")
            return None

        if self.calculate_checksum(json_part) != expected_checksum:
            print(f"Checksum mismatch. '{message}'")
            return None

        try:
            return json.loads(json_part)
        except json.JSONDecodeError:
            print(f"Failed to decode JSON. '{message}'")
            return None

    def ping(self, address):
        self.send_message(address, "ping", {})

    def discover_devices(self):
        self.send_message("broadcast", "discover", {})


# Example usage
if __name__ == "__main__":
    def handle_message(data):
        pass
        # print(f"Callback received data: {data}")

    rs_json = None  # Initialize rs_json to None for the finally block

    try:
        ports = list(serial.tools.list_ports.comports())
        if ports:
            print(f"Using serial port: {ports[0].device}")
            rs_json = RSJSON(ports[0].device, 115200)
        else:
            raise Exception("No serial ports found.")

        rs_json.set_callback(handle_message)
        rs_json.discover_devices()

        last_read_command_time = time.time()
        read_command_interval = 1.0  # Send "read" command every 1 second
        try:
            counter = 0
            while True:
                current_time = time.time()
                if current_time - last_read_command_time >= read_command_interval:
                    rs_json.send_message("1", "ping", {})
                    last_read_command_time = current_time
                time.sleep(0.01)  # Small delay to prevent busy-waiting and allow other operations
        except KeyboardInterrupt:
            rs_json.send_message("1", "disable", {})  # Disable acceptor
            print("Program terminated by user.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        if rs_json and rs_json.serial.is_open:
            print("Closing serial port.")
            rs_json.serial.flush()
            rs_json.serial.close()
