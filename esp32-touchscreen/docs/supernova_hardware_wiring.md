# Supernova hardware wiring

Target controller: diymore/LCDWiki 4.0 inch ESP32-32E touchscreen display.

Known pins already used by the display board:

| Function | ESP32 GPIO |
| --- | --- |
| LCD CS | GPIO15 |
| LCD DC | GPIO2 |
| LCD SCLK | GPIO14 |
| LCD MOSI | GPIO13 |
| LCD MISO | GPIO12 |
| LCD backlight | GPIO27 |
| Touch CS | GPIO33 |
| Touch IRQ | GPIO36 |
| microSD CS | GPIO5 |
| microSD SCLK | GPIO18 |
| microSD MOSI | GPIO23 |
| microSD MISO | GPIO19 |

## Proposed extra pins

| Device | Signal | ESP32 GPIO |
| --- | --- | --- |
| MAX98357A amp pair | I2S BCLK | GPIO25 |
| MAX98357A amp pair | I2S LRC / LRCLK | GPIO32 |
| MAX98357A amp pair | I2S DIN | GPIO26 |
| 109 LED ring | Data in | GPIO21 |
| Servo 1 | PWM signal | TBD |
| Servo 2 | PWM signal | TBD |

## Audio

Use two MAX98357A amplifier boards for two speakers. Do not connect speakers directly to the ESP32.

For both MAX98357A boards:

| MAX98357A pin | Connect to |
| --- | --- |
| VIN | 5V external supply |
| GND | Common ground with ESP32 |
| BCLK | ESP32 GPIO25 |
| LRC | ESP32 GPIO32 |
| DIN | ESP32 GPIO26 |
| Speaker + / - | One 8 ohm speaker |

Because the MAX98357A boards are mono, the clean setup is one amplifier per speaker. Feed both amplifiers the same I2S signals for dual-mono audio. Avoid connecting two 8 ohm speakers in parallel to one amp because that becomes 4 ohm and can overload the module.

## LED ring

The 109-pixel 5V addressable LED ring needs a separate 5V supply.

| LED ring pin | Connect to |
| --- | --- |
| 5V | External 5V supply |
| GND | External supply ground and ESP32 GND |
| DIN | ESP32 GPIO21 through 330 ohm resistor |

Recommended protection:

- 1000 uF capacitor across LED 5V and GND near the ring.
- Logic level shifter from ESP32 3.3V data to 5V data if the ring is unreliable.
- Power budget: 109 LEDs can draw about 6.5A at full white. Use a 5V supply rated around 8A or more if you want full brightness.

## Servos

The RDS3218 servos must use a separate servo power supply/BEC. Do not power them from the ESP32 board.

Do not use GPIO18 or GPIO19 for servos on this display board. Those pins are used by the on-board microSD card, so using them for servo PWM will break SD music playback. A clean future option is an external PCA9685 servo driver on the board's I2C pins.

| Servo wire | Connect to |
| --- | --- |
| Servo 1 signal | Future servo driver channel 1 |
| Servo 2 signal | Future servo driver channel 2 |
| Servo V+ | External servo supply, usually 6V to 8.4V depending on your servo rating |
| Servo GND | External supply ground and ESP32 GND |

Use a high-current supply. Two 20 kg servos can pull large current spikes, so plan for several amps per servo.

## Shared ground rule

All external supplies must share ground with the ESP32 touchscreen:

ESP32 GND, LED power GND, servo power GND, and audio amp GND must be connected together.

## ESP32-CAM later

The touchscreen will store WiFi settings. Later, the ESP32-CAM can either:

- join the same WiFi using the same SSID/password, or
- boot into a temporary setup mode where the touchscreen sends WiFi credentials over a local HTTP/BLE/provisioning link.

The current firmware contains only the camera UI placeholder and servo control overlay stubs.
