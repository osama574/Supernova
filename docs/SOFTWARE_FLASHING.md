# Software Setup And Flashing

This repository contains two separate PlatformIO projects.

| Folder | Device | PlatformIO Environment |
| --- | --- | --- |
| `esp32-touchscreen` | diymore/LCDWiki 4.0 inch ESP32-32E touchscreen | `esp32_32e_display` |
| `esp32-camera` | ESP32-CAM AI-Thinker style board | `esp32cam_ai_thinker` |

## Install Requirements

Required:

- VS Code
- PlatformIO extension for VS Code, or PlatformIO CLI
- USB cable for the touchscreen
- USB programmer/adapter for the ESP32-CAM

If `pio` is not available in PowerShell, use the VS Code PlatformIO terminal or this full path:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
```

## Configure WiFi

The GitHub copy uses placeholders.

Touchscreen file:

```text
esp32-touchscreen/src/supernova_state.cpp
```

Camera file:

```text
esp32-camera/src/main.cpp
```

Change:

```cpp
static const char * default_wifi_ssid = "CHANGE_ME_WIFI_SSID";
static const char * default_wifi_password = "CHANGE_ME_WIFI_PASSWORD";
```

The touchscreen can also change WiFi from the Settings app. Successful WiFi credentials are saved in ESP32 flash.

## Configure MQTT Broker IP

The touchscreen connects directly to the MQTT broker. If the broker runs on your laptop, use the laptop IP address on the same WiFi network.

Edit:

```text
esp32-touchscreen/platformio.ini
```

Change:

```ini
-DSUPERNOVA_MQTT_HOST=\"CHANGE_ME_BROKER_IP\"
```

Example:

```ini
-DSUPERNOVA_MQTT_HOST=\"172.20.10.2\"
```

Do not use `localhost` for the ESP32. On the ESP32, `localhost` means the ESP32 itself, not the laptop.

## Build Touchscreen Firmware

PowerShell:

```powershell
cd "C:\path\to\Supernova\esp32-touchscreen"
pio run -e esp32_32e_display
```

If `pio` is not found:

```powershell
cd "C:\path\to\Supernova\esp32-touchscreen"
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32_32e_display
```

Expected result:

```text
[SUCCESS]
```

## Upload Touchscreen Firmware

Find the COM port in Windows Device Manager or PlatformIO devices. Example port:

```text
COM6
```

Upload:

```powershell
cd "C:\path\to\Supernova\esp32-touchscreen"
pio run -e esp32_32e_display -t upload --upload-port COM6
```

Serial monitor:

```powershell
pio device monitor -e esp32_32e_display --port COM6
```

Expected after boot:

- Supernova boot animation
- Home dashboard
- Touch works in landscape

## Build ESP32-CAM Firmware

PowerShell:

```powershell
cd "C:\path\to\Supernova\esp32-camera"
pio run -e esp32cam_ai_thinker
```

Expected result:

```text
[SUCCESS]
```

## Upload ESP32-CAM Firmware

Example port:

```text
COM4
```

Upload:

```powershell
cd "C:\path\to\Supernova\esp32-camera"
pio run -e esp32cam_ai_thinker -t upload --upload-port COM4
```

Serial monitor:

```powershell
pio device monitor -e esp32cam_ai_thinker --port COM4
```

ESP32-CAM bootloader mode:

- If upload does not start, hold `IO0`.
- Press `RST`.
- Start upload.
- Release `IO0` after upload begins.

Some ESP32-CAM-MB boards have an `IO0` button instead of requiring a jumper.

## Erase Flash

Erase flash if old WiFi credentials or settings are stuck:

Touchscreen:

```powershell
cd "C:\path\to\Supernova\esp32-touchscreen"
pio run -e esp32_32e_display -t erase --upload-port COM6
```

Camera:

```powershell
cd "C:\path\to\Supernova\esp32-camera"
pio run -e esp32cam_ai_thinker -t erase --upload-port COM4
```

Then upload firmware again.

## What The Firmware Does

Touchscreen firmware:

- Initializes ST7796S LCD in landscape
- Initializes XPT2046 touch
- Runs LVGL 9 UI
- Shows Supernova boot animation
- Provides dashboard apps
- Saves theme/language/brightness/WiFi settings
- Reads WAV music from SD card
- Controls two servo motors
- Controls LED ring
- Receives camera IP state
- Subscribes to MQTT motor commands

Camera firmware:

- Initializes OV2640 camera
- Connects to WiFi
- Receives WiFi credentials over ESP-NOW
- Starts HTTP camera server
- Announces IP address to touchscreen

