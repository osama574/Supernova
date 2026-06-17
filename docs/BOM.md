# Bill Of Materials

This is the hardware needed to recreate the current Supernova prototype.

## Required Electronics

| Qty | Part | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | diymore/LCDWiki 4.0 inch ESP32-32E touchscreen display | Main UI controller | 320 x 480 display, used in landscape as 480 x 320 |
| 1 | ESP32-CAM AI-Thinker style board with OV2640 | Camera controller | Tested with ESP32-CAM-MB style USB programmer |
| 2 | RDS3218 20 kg 270 degree servos | Pan/tilt movement | Needs external servo power |
| 1 | 109 pixel WS2812/NeoPixel compatible LED ring | Addressable LED effect | Needs external 5V power |
| 1 | microSD card | Music storage | FAT32, `/music` folder |
| 1 | Speaker or speaker module supported by the display board | Audio output | Current firmware uses ESP32 DAC output on GPIO26 |
| 1 | Laptop | PlatformIO upload, Node-RED, MQTT broker | Windows commands are documented |

## Optional Electronics

| Qty | Part | Purpose | Notes |
| --- | --- | --- | --- |
| 1 or 2 | MAX98357A amplifier modules | External audio amplifier option | Current firmware does not drive external I2S MAX98357A. Code changes are needed for this option. |
| 1 | PCA9685 servo driver | Cleaner multi-servo control | Recommended future upgrade if more PWM outputs are needed. |

## Power Supplies

| Supply | Recommended Rating | Used For |
| --- | --- | --- |
| USB-C or 5V supply for touchscreen | Stable 5V, at least 1A | ESP32 display board |
| LED supply | 5V, up to 8A for full brightness | 109 LED ring |
| Servo supply/BEC | Match servo rating, several amps per servo | Two high-current servos |

The LED and servo supplies can draw far more current than the ESP32 board can provide. Keep power separate, but connect grounds together.

## Mechanical Parts

Print these files from the repository:

| File | Purpose |
| --- | --- |
| `3D-Model/pantilt_floor_nolegs.stl` | Base/floor part |
| `3D-Model/pantilt_leg.stl` | Leg/support part |
| `3D-Model/pantilt_screenpanel.stl` | Screen panel mount |

See `docs/3D_PRINTING_AND_ASSEMBLY.md` for print and assembly steps.

## Tools

- 3D printer
- Slicer software
- Small screwdrivers
- Soldering iron or reliable Dupont/JST wiring
- Multimeter
- USB cables for both ESP32 devices
- Heat shrink or insulation tape
- Computer with VS Code and PlatformIO

