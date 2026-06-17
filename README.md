# Supernova ESP32 Smart Display

Supernova is a two-device ESP32 smart display project:

- `esp32-touchscreen`: LVGL touchscreen firmware for a diymore/LCDWiki 4.0 inch ESP32-32E display.
- `esp32-camera`: ESP32-CAM firmware for an AI-Thinker style ESP32-CAM with OV2640.
- `node-red`: Node-RED flow for simple MQTT motor control from a laptop dashboard.

The touchscreen is the main UI. It shows the Supernova boot animation, home dashboard, WiFi/settings, music, weather, clock, calendar, calculator, timer, and motor control. The camera firmware can join the same WiFi network, announce its IP to the display, and expose HTTP camera endpoints. Node-RED can send MQTT commands to move the two servos.

## Hardware

### Touchscreen firmware

Folder:

```text
esp32-touchscreen
```

Intended device:

- diymore / LCDWiki 4.0 inch ESP32-32E display
- 320 x 480 LCD, used in landscape as 480 x 320
- ST7796S display over SPI
- XPT2046 resistive touch over SPI
- LVGL 9 based UI

Connected extras used by the firmware:

- MAX98357A I2S audio amplifier and speaker
- microSD card for WAV music files
- WS2812/NeoPixel style LED ring
- Two servo motors for pan/tilt control

Hardware notes are in:

```text
esp32-touchscreen/docs/supernova_hardware_wiring.md
```

### Camera firmware

Folder:

```text
esp32-camera
```

Intended device:

- ESP32-CAM / AI-Thinker style module
- OV2640 camera
- PSRAM enabled board profile through PlatformIO `esp32cam`

The camera firmware provides:

- WiFi connection
- ESP-NOW credential receiving from the touchscreen
- HTTP status and camera endpoints
- Camera IP announcement back to the touchscreen

Useful endpoints after the ESP32-CAM is connected:

```text
http://CAMERA_IP/status
http://CAMERA_IP/capture
http://CAMERA_IP/stream
```

## Required Software

- VS Code with PlatformIO extension, or PlatformIO CLI
- Arduino framework through PlatformIO
- Node-RED already installed, if you want the laptop motor dashboard
- MQTT broker on the laptop, for example Mosquitto

## Configuration To Change

Before publishing or flashing your own copy, check these values.

### WiFi defaults

The GitHub copy uses placeholders so real WiFi credentials are not published.

Touchscreen:

```text
esp32-touchscreen/src/supernova_state.cpp
```

Camera:

```text
esp32-camera/src/main.cpp
```

Change:

```cpp
static const char * default_wifi_ssid = "CHANGE_ME_WIFI_SSID";
static const char * default_wifi_password = "CHANGE_ME_WIFI_PASSWORD";
```

The touchscreen also lets you connect to WiFi from the Settings UI. Successful WiFi credentials are saved in ESP32 flash. The ESP32-CAM can also receive WiFi credentials from the touchscreen over ESP-NOW.

If a device already has old saved credentials, use the touchscreen WiFi UI to connect to the new network, or erase flash from PlatformIO.

### MQTT broker IP

The ESP32 touchscreen must connect to the laptop's network IP address, not `localhost`.

Change this in:

```text
esp32-touchscreen/platformio.ini
```

Example:

```ini
-DSUPERNOVA_MQTT_HOST=\"172.20.10.2\"
```

If your laptop IP changes, update this value and upload the touchscreen firmware again.

Node-RED can use `127.0.0.1` if Node-RED and the MQTT broker are on the same laptop. If the broker is on another computer, change the broker in the Node-RED MQTT config node.

### Serial ports

The upload ports are examples and may be different on your computer:

- Touchscreen was tested on `COM6`
- ESP32-CAM was tested on `COM4`

Check Windows Device Manager or PlatformIO devices if your ports differ.

## Build And Upload: Touchscreen

From PowerShell:

```powershell
cd "C:\path\to\Supernova-ESP32-GitHub\esp32-touchscreen"
pio run -e esp32_32e_display
pio run -e esp32_32e_display -t upload --upload-port COM6
```

Serial monitor:

```powershell
pio device monitor -e esp32_32e_display --port COM6
```

If `pio` is not found but PlatformIO is installed by VS Code, use:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32_32e_display
```

## Build And Upload: ESP32-CAM

From PowerShell:

```powershell
cd "C:\path\to\Supernova-ESP32-GitHub\esp32-camera"
pio run -e esp32cam_ai_thinker
pio run -e esp32cam_ai_thinker -t upload --upload-port COM4
```

Serial monitor:

```powershell
pio device monitor -e esp32cam_ai_thinker --port COM4
```

If uploading fails, put the ESP32-CAM in bootloader mode. On many ESP32-CAM-MB adapter boards this means holding the `IO0` button, pressing reset, then starting upload. Release `IO0` after upload starts.

## Node-RED Motor Control

Flow file:

```text
node-red/nodered-supernova-motors-flow.json
```

Import:

1. Open Node-RED.
2. Menu -> Import.
3. Select the JSON file above or paste its contents.
4. Deploy.
5. Open the Node-RED dashboard.

The flow publishes servo commands to:

```text
supernova/motors/cmd
```

Payloads:

```text
up
down
left
right
```

The touchscreen publishes servo state to:

```text
supernova/motors/state
```

Example Mosquitto config:

```text
node-red/mosquitto-supernova.conf
```

For the ESP32 to reach the broker, Mosquitto must listen on the laptop network interface, not only on `localhost`. The included config uses:

```conf
listener 1883 0.0.0.0
allow_anonymous true
```

You may also need to allow port `1883` in Windows Firewall.

## Music Files

Put music files on the touchscreen SD card in:

```text
/music
```

The current firmware is focused on WAV playback. Use FAT32 for the SD card.

## Repository Layout

```text
Supernova-ESP32-GitHub/
  README.md
  .gitignore
  esp32-touchscreen/
    platformio.ini
    include/
    lib/
    scripts/
    src/
    docs/
  esp32-camera/
    platformio.ini
    src/
  node-red/
    nodered-supernova-motors-flow.json
    mosquitto-supernova.conf
```
