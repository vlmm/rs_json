Python библиотека, която реализира комуникация по RS485 с Master-Slave архитектура, използвайки JSON формат и контролна сума, както е описано в предишната документация. Библиотеката включва основни функции за инициализация, изпращане на съобщения и обработка на отговори.

### `rs_json.py`

```python
import json
import serial

class RSJSON:
    def __init__(self, port, baudrate):
        """Инициализира серийната комуникация."""
        self.serial = serial.Serial(port, baudrate)
    
    def calculate_checksum(self, message):
        """Изчислява контролната сума на съобщението."""
        return sum(message.encode('utf-8')) % 256

    def send_message(self, address, command, data):
        """Изпраща съобщение до конкретно устройство."""
        message = {
            "address": address,
            "command": command,
            "data": data
        }
        message_str = json.dumps(message)
        checksum = self.calculate_checksum(message_str)
        message_str += f"{checksum:02d}"  # Форматиране на контролната сума с две цифри
        
        self.serial.write(message_str.encode('utf-8'))

    def listen(self):
        """Слуша за входящи съобщения от устройства."""
        if self.serial.in_waiting > 0:
            message = self.serial.readline().decode('utf-8').strip()
            return self.process_message(message)
        return None

    def process_message(self, message):
        """Обработва полученото съобщение и проверява контролната сума."""
        json_part = message[:-2]  # Извличане на JSON част
        checksum_part = message[-2:]  # Извличане на контролната сума

        # Проверка на контролната сума
        if self.calculate_checksum(json_part) != int(checksum_part):
            print("Невалидна контролна сума.")
            return None

        try:
            data = json.loads(json_part)
            return data
        except json.JSONDecodeError:
            print("Грешка при декодиране на JSON.")
            return None

    def ping(self, address):
        """Изпраща пинг команда до конкретно устройство."""
        self.send_message(address, "ping", {})

    def discover_devices(self):
        """Изпраща команда за откриване на устройства."""
        self.send_message("broadcast", "discover", {})

# Пример за използване на библиотеката
if __name__ == "__main__":
    rs_json = RSJSON('/dev/ttyUSB0', 9600)
    
    # Откриване на устройства
    rs_json.discover_devices()
    
    # Изпращане на команда
    rs_json.send_message("slave_1", "set_value", {"value": 10})
    
    # Слушане за отговори
    while True:
        response = rs_json.listen()
        if response:
            print("Получен отговор:", response)
```

### Обяснение на библиотеката:

1. **Инициализация**: Конструкторът `__init__` инициализира серийната комуникация с посочения порт и скорост на предаване.

2. **Изчисляване на контролна сума**: Методът `calculate_checksum` изчислява контролната сума на съобщението, като сумира ASCII стойностите на символите.

3. **Изпращане на съобщения**: Методът `send_message` форматира съобщението в JSON, добавя контролната сума и го изпраща по серийния порт.

4. **Слушане за входящи съобщения**: Методът `listen` проверява за налични съобщения и обработва получените данни.

5. **Обработка на съобщения**: Методът `process_message` проверява контролната сума и декодира JSON частта.

6. **Пинг и откриване на устройства**: Методите `ping` и `discover_devices` изпращат съответните команди до устройствата.

### Пример за използване:
В края на файла има пример за използване на библиотеката, който демонстрира как да се открият устройства, да се изпрати команда и да се слуша за отговори.

Тази библиотека предоставя основна функционалност за работа с сериен интерфейс, използвайки JSON формат и контролна сума, и може да бъде разширена с допълнителни функции в зависимост от нуждите на проекта.