# Troubleshooting

Use this guide when the system does not behave as expected.

## PlatformIO Cannot Find `pio`

Use the full PlatformIO path:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```

Or open the project folder in VS Code and use the PlatformIO extension.

## Upload Fails

Check:

- Correct COM port.
- USB cable supports data.
- No serial monitor is already using the same COM port.
- Correct PlatformIO environment.

Touchscreen:

```powershell
pio run -e esp32_32e_display -t upload --upload-port COM6
```

ESP32-CAM:

```powershell
pio run -e esp32cam_ai_thinker -t upload --upload-port COM4
```

For ESP32-CAM, hold `IO0`, press `RST`, start upload, then release `IO0` after upload begins.

## Touchscreen Stays White Or Blank

Possible causes:

- Wrong display initialization.
- Firmware did not boot.
- Power problem.
- Wrong project uploaded to the board.

Actions:

1. Open serial monitor at 115200 baud.
2. Confirm boot logs appear.
3. Rebuild and upload `esp32-touchscreen`.
4. Confirm the target environment is `esp32_32e_display`.
5. Check USB power stability.

## Touch Coordinates Are Wrong

Symptoms:

- Pressing right triggers left.
- Pressing top triggers bottom.
- Buttons respond offset from finger.

Actions:

1. Confirm the firmware is the current landscape version.
2. Check touch logs in serial monitor.
3. Adjust mapping in:

```text
esp32-touchscreen/src/main.cpp
```

Touch mapping is inside `lvgl_touch_read_cb`.

## Songs Do Not Show

Check SD card:

- Format: FAT32
- Folder: `/music`
- Files: `.wav`
- WAV format: PCM, 8-bit or 16-bit, mono or stereo

The firmware ignores MP3 files.

The firmware detects up to 120 WAV files.

If the card was recently reformatted:

1. Create folder `music` at the SD card root.
2. Put WAV files directly inside it.
3. Reinsert card.
4. Restart device.
5. Open Music app.

## Song Plays Slow Or Distorted

Possible causes:

- Unsupported WAV encoding.
- Very high sample rate.
- Weak speaker/amplifier wiring.
- Power noise.

Recommended test file:

- WAV PCM
- 16-bit
- 44100 Hz or 22050 Hz
- mono or stereo

## MQTT Node-RED Says Connecting

This means Node-RED cannot connect to the MQTT broker.

Check broker locally:

```powershell
Test-NetConnection 127.0.0.1 -Port 1883
```

Check broker from laptop IP:

```powershell
Test-NetConnection YOUR_LAPTOP_IP -Port 1883
```

Fixes:

- Start Mosquitto.
- Use `node-red/mosquitto-supernova.conf`.
- Allow port 1883 in Windows Firewall.
- In Node-RED, set broker to `127.0.0.1` if broker and Node-RED are on the same laptop.

## ESP32 Shows `MQTT: retry`

This means the touchscreen cannot connect to the MQTT broker.

Check:

- Touchscreen WiFi connected.
- Laptop and touchscreen are on the same WiFi/hotspot.
- `SUPERNOVA_MQTT_HOST` is the laptop network IP, not `localhost`.
- MQTT broker listens on `0.0.0.0:1883`.
- Firewall allows incoming TCP 1883.

After changing `SUPERNOVA_MQTT_HOST`, rebuild and upload the touchscreen firmware.

## Servos Do Not Move

Check:

- Servo supply is on.
- Servo supply voltage matches servo rating.
- Servo GND is connected to ESP32 GND.
- Pan signal is GPIO25.
- Tilt signal is GPIO32.
- You are in the Motor app.

If testing from Node-RED:

- MQTT broker connected.
- Topic is `supernova/motors/cmd`.
- Payload is `up`, `down`, `left`, or `right`.

## Servos Move Randomly Or Vibrate

Common causes:

- Weak servo power supply.
- No shared ground.
- Loose signal wire.
- Servo horn mechanically blocked.
- Signal wire near noisy power wires.

Actions:

1. Disconnect servo power.
2. Confirm wiring.
3. Connect grounds together.
4. Test one servo at a time.
5. Keep servo power wires away from signal wires.

## ESP32-CAM Shows No Camera Frame

Common causes:

- Camera ribbon cable not seated.
- Wrong camera board model.
- Weak power.
- ESP32-CAM not in correct firmware.
- OV2640 module issue.

Actions:

1. Reseat the camera ribbon cable.
2. Use stable 5V power.
3. Reflash `esp32-camera`.
4. Open serial monitor.
5. Check:

```text
http://CAMERA_IP/status
http://CAMERA_IP/capture
```

If `/status` works but `/capture` fails, the WiFi server is alive but the sensor/camera path is failing.

## ESP32-CAM IP Changes

The IP address is assigned by the router/hotspot DHCP server. It can change.

Options:

- Check serial monitor during development.
- Look in the touchscreen Motor app, which shows the camera IP when the camera announces itself.
- Reserve a DHCP address in the router/hotspot if available.

## Weather Or Clock Does Not Update

Check:

- WiFi connected.
- Internet is available.
- Time sync may take a little while after boot.
- Weather needs internet access to call the Open-Meteo API.

If WiFi disconnects after time sync, the clock keeps running from the ESP32 runtime clock.

