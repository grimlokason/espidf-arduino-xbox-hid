# ESP32 Xbox BLE HID (minimal)

Projet minimal pour exposer une manette Xbox BLE HID sur ESP32-C6 via Arduino + ESP-IDF.

## Dépendances

Les dépendances sont déclarées dans `platformio.ini` :
- `h2zero/esp-nimble-cpp`
- `ESP32-BLE-CompositeHID` (GitHub)

## Installation

1. Installer PlatformIO Core :
```bash
pip install -U platformio
```

2. Installer / résoudre les dépendances du projet :
```bash
pio pkg install -e esp32-c6-devkitc-1
```

3. Compiler :
```bash
pio run -e esp32-c6-devkitc-1
```

4. Flasher :
```bash
pio run -e esp32-c6-devkitc-1 -t upload
```
