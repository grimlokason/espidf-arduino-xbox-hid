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

## Flasher depuis WSL

WSL2 n'a pas d'accès direct aux périphériques USB de Windows. Il faut passer le port série (USB) de l'ESP32 via `usbipd-win`.

1. Sur Windows (PowerShell en administrateur), installer `usbipd-win` :
```powershell
winget install usbipd
```

2. Brancher l'ESP32, puis lister les périphériques USB pour repérer son `BUSID` :
```powershell
usbipd list
```

3. Partager (une seule fois) puis attacher le périphérique à WSL :
```powershell
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```
> À refaire après chaque débranchement/rebranchement ou redémarrage (`usbipd attach` suffit une fois le `bind` fait).

4. Dans WSL, vérifier que le périphérique est bien visible :
```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

5. Si besoin, donner l'accès au port sans `sudo` (ajout au groupe `dialout`, puis se reconnecter/redémarrer WSL) :
```bash
sudo usermod -aG dialout $USER
```

6. Compiler et flasher en précisant le port détecté :
```bash
pio run -e esp32-c6-devkitc-1 -t upload --upload-port /dev/ttyUSB0
```

7. Ouvrir le moniteur série :
```bash
pio device monitor -p /dev/ttyUSB0
```
