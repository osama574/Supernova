# Start Here: Recreate The Supernova Project

This document is the handoff guide for rebuilding Supernova from the repository with no extra help. Follow the sections in order.

## What You Are Building

Supernova is a smart display system made from two ESP32 devices:

1. Main display controller
   - diymore/LCDWiki 4.0 inch ESP32-32E touchscreen display
   - Runs the LVGL user interface
   - Controls music, settings, WiFi, weather, clock, calendar, timer, calculator, servos, LED ring, and MQTT motor commands

2. Camera controller
   - ESP32-CAM AI-Thinker style board with OV2640 camera
   - Joins the same WiFi network
   - Provides HTTP camera endpoints
   - Announces its IP address to the touchscreen

There is also a Node-RED dashboard flow for controlling the servo motors over MQTT from a laptop.

## Repository Map

```text
Supernova/
  README.md
  docs/
    START_HERE_RECREATE.md
    BOM.md
    3D_PRINTING_AND_ASSEMBLY.md
    WIRING.md
    SOFTWARE_FLASHING.md
    NODE_RED_MQTT.md
    OPERATION_TEST_PLAN.md
    TROUBLESHOOTING.md
  3D-Model/
    pantilt_floor_nolegs.stl
    pantilt_leg.stl
    pantilt_screenpanel.stl
  esp32-touchscreen/
    platformio.ini
    src/
    include/
    scripts/
  esp32-camera/
    platformio.ini
    src/
  node-red/
    nodered-supernova-motors-flow.json
    mosquitto-supernova.conf
```

## Build Order

1. Read `docs/BOM.md` and confirm all parts are available.
2. Print the case parts from `3D-Model/`; follow `docs/3D_PRINTING_AND_ASSEMBLY.md`.
3. Wire the electronics according to `docs/WIRING.md`.
4. Edit WiFi and MQTT settings as described in `docs/SOFTWARE_FLASHING.md`.
5. Build and upload the touchscreen firmware.
6. Build and upload the ESP32-CAM firmware.
7. Import the Node-RED flow using `docs/NODE_RED_MQTT.md`.
8. Run the acceptance test in `docs/OPERATION_TEST_PLAN.md`.
9. If something fails, use `docs/TROUBLESHOOTING.md`.

## Expected Result

After everything is working:

- The touchscreen boots into the Supernova animation and home dashboard.
- Touch works in landscape orientation.
- Settings can connect/disconnect WiFi.
- Music lists WAV files from the SD card.
- The Motor app moves pan/tilt servos from touch buttons.
- The Motor app shows the ESP32-CAM IP when the camera is available.
- Node-RED can move the same motors by publishing MQTT commands.
- The ESP32-CAM exposes camera endpoints on the local network.

## Important Safety Rules

- Do not power the 20 kg servos from the ESP32 display board.
- Do not power the LED ring from the ESP32 display board.
- Use a separate high-current 5V supply for LEDs.
- Use a separate high-current servo supply/BEC for servos.
- Connect all grounds together: ESP32 GND, LED GND, servo supply GND, and audio GND.
- Check polarity before powering the system.
- If a servo moves randomly, remove servo power first, then debug wiring and ground.

