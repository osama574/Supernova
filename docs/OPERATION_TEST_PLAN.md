# Operation And Acceptance Test Plan

Use this checklist after assembly and flashing. Do not skip steps; each step proves one part of the system.

## 1. First Power-On

1. Connect only the touchscreen by USB.
2. Power on.
3. Confirm the boot animation appears.
4. Confirm the home dashboard appears.
5. Confirm the screen is landscape.

Pass condition:

```text
Supernova boot animation -> home dashboard
```

## 2. Touch Test

1. Tap Settings.
2. Tap back.
3. Tap Motor app.
4. Tap each arrow button.

Pass condition:

- Taps select the intended buttons.
- Left side does not press right side.
- Up/down are not inverted.

## 3. WiFi Test

1. Open Settings.
2. Open WiFi setup.
3. Enter SSID and password.
4. Tap Connect.
5. Return to home.

Pass condition:

- Top bar shows connected WiFi icon.
- Clock eventually updates from internet.
- Weather can load.

## 4. SD Music Test

Prepare SD card:

```text
FAT32
/music
/music/example.wav
```

Supported WAV:

- PCM
- 8-bit or 16-bit
- mono or stereo

Test:

1. Insert SD card.
2. Boot touchscreen.
3. Open Music app.
4. Confirm songs appear.
5. Play one song.

Pass condition:

- Song list appears.
- Audio plays.
- UI stays responsive.

## 5. Servo Touchscreen Test

1. Power servo supply.
2. Confirm servo GND and ESP32 GND are connected.
3. Open Motor app.
4. Press left, right, up, and down.

Pass condition:

- Pan servo moves left/right.
- Tilt servo moves up/down.
- Each tap moves about 2 degrees.
- Holding a button repeats movement.
- No random movement when buttons are not pressed.

## 6. LED Ring Test

1. Connect LED ring 5V supply.
2. Connect LED GND to ESP32 GND.
3. Connect DIN to GPIO21.
4. Open Settings.
5. Change LED mode.

Pass condition:

- LED mode changes according to firmware setting.
- No ESP32 reset when LEDs turn on.

## 7. ESP32-CAM Test

1. Flash ESP32-CAM firmware.
2. Open serial monitor.
3. Wait for WiFi connection.
4. Note the camera IP.
5. Open browser:

```text
http://CAMERA_IP/status
http://CAMERA_IP/capture
http://CAMERA_IP/stream
```

Pass condition:

- `/status` returns JSON.
- `/capture` returns an image.
- `/stream` returns camera stream.
- Motor app on touchscreen shows CAM IP.

## 8. Node-RED MQTT Motor Test

1. Start MQTT broker.
2. Deploy Node-RED flow.
3. Confirm Node-RED MQTT node says connected.
4. Open Node-RED dashboard.
5. Press each motor button.

Pass condition:

- Servos move from Node-RED buttons.
- Touchscreen Motor app shows `MQTT: online`.
- State updates appear on topic `supernova/motors/state`.

## 9. Final Full-System Test

With everything connected:

1. Power touchscreen.
2. Power servo supply.
3. Power LED supply.
4. Start MQTT broker.
5. Start Node-RED.
6. Power ESP32-CAM.
7. Verify home dashboard.
8. Verify WiFi icon.
9. Verify music list.
10. Verify motor control from touchscreen.
11. Verify motor control from Node-RED.
12. Verify camera IP is visible.

The project is accepted only when all checks pass.

