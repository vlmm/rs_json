#
#  {"address": "address", "command": "ping", "data": "data"}
#
import json
import serial
import serial.tools.list_ports
import time
import threading


class RSJSON:
    def __init__(self, port, baudrate):
        self.serial = serial.Serial(port, baudrate, timeout=1)
        self.callback = None

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

    def listen(self):
        if self.serial.in_waiting > 0:
            line = self.serial.readline().decode('utf-8', errors='replace').strip()
            data = self.process_message(line)
            if data and self.callback:
                self.callback(data)
            return data
        return None

    def process_message(self, message):
        print(f"Message received: '{message}'")

        # Проверка за дължина на съобщението
        if len(message) < 2:
            print("Invalid message format.")
            return None

        # Извличане на JSON част и контролната сума
        json_part = message[:-2]  # Всичко освен последните два символа
        checksum_str = message[-2:]  # Последните два символа

        try:
            expected_checksum = int(checksum_str, 16)  # Преобразуване от шестнадесетично
        except ValueError:
            print("Invalid checksum format.")
            return None

        if self.calculate_checksum(json_part) != expected_checksum:
            print("Checksum mismatch.")
            return None

        try:
            return json.loads(json_part)
        except json.JSONDecodeError:
            print("Failed to decode JSON.")
            return None

    def ping(self, address):
        self.send_message(address, "ping", {})

    def discover_devices(self):
        self.send_message("broadcast", "discover", {})

# Example usage
if __name__ == "__main__":
    def handle_message(data):
        pass
        #print(f"Callback received data: {data}")

    rs_json = None # Initialize rs_json to None for the finally block

    try:
        ports = list(serial.tools.list_ports.comports())
        if ports:
            print(f"Using serial port: {ports[0].device}")
            rs_json = RSJSON(ports[0].device, 115200)
        else:
            raise Exception("No serial ports found.")

        rs_json.set_callback(handle_message)
        rs_json.discover_devices()
        rs_json.send_message("1", "ping", {}) # Send initial ping

        last_read_command_time = time.time()
        read_command_interval = 1.0 # Send "read" command every 1 second

        try:
            while True:
                current_time = time.time()
                if current_time - last_read_command_time >= read_command_interval:
                    rs_json.send_message("1", "read", {})
                    last_read_command_time = current_time

                rs_json.listen() # Continuously listen for incoming messages
                time.sleep(0.01) # Small delay to prevent busy-waiting and allow other operations
        except KeyboardInterrupt:
            print("Program terminated by user.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        if rs_json and rs_json.serial.is_open:
            print("Closing serial port.")
            rs_json.serial.close()

