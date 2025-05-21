# RS JSON Library

## Description (English)
The "RS JSON" library provides an interface for working with RS485 communication, which supports both Master and Slave devices. It uses JSON message format, which facilitates data exchange between devices.

## Описание (Български)
Библиотеката "RS JSON" предоставя интерфейс за работа с RS485 комуникация, който поддържа както Master, така и Slave устройства. Използва JSON формат за съобщения, което улеснява обмена на данни между устройствата.

---

## Project Structure

```
/RS_JSON
│
├── /src
│ ├── RS_JSON.h
│ ├── RS_JSON.cpp
│
└── /examples
    ├── /master
    │ └── master.ino
    └── /slave
        └── slave.ino
```

---

## Requirements (English)
- Arduino IDE
- [ArduinoJson] Library (https://github.com/bblanchon/ArduinoJson) (can be installed via Library Manager in Arduino IDE)

## Изисквания (Български)
- Arduino IDE
- Библиотека [ArduinoJson](https://github.com/bblanchon/ArduinoJson) (може да бъде инсталирана чрез Library Manager в Arduino IDE)

---

## Installation (English)
1. Create a directory named `RS_JSON` in the `libraries` folder of your Arduino installation.
2. Add the `RS_JSON.h` and `RS_JSON.cpp` files to the `src` subdirectory.
3. Create a `examples` subdirectory and add the `MasterExample` and `SlaveExample` examples.

## Инсталация (Български)
1. Създайте директория с името `RS_JSON` в папката `libraries` на вашата Arduino инсталация.
2. Добавете файловете `RS_JSON.h` и `RS_JSON.cpp` в поддиректория `src`.
3. Създайте поддиректория `examples` и добавете примерите `MasterExample` и `SlaveExample`.
