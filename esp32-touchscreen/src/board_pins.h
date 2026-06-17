#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/* LCDWiki/diymore 4.0inch ESP32-32E Display, touch SKU E32R40T. */
#define LCD_HOST_SPI HSPI_HOST

#define LCD_PIN_CS 15
#define LCD_PIN_DC 2
#define LCD_PIN_SCLK 14
#define LCD_PIN_MOSI 13
#define LCD_PIN_MISO 12
#define LCD_PIN_RST -1
#define LCD_PIN_BL 27

#define TOUCH_PIN_CS 33
#define TOUCH_PIN_IRQ 36

#define SD_PIN_CS 5
#define SD_PIN_SCLK 18
#define SD_PIN_MOSI 23
#define SD_PIN_MISO 19

/* Optional on-board peripherals, not used by the first UI version. */
#define RGB_LED_RED 22
#define RGB_LED_GREEN 16
#define RGB_LED_BLUE 17
#define BATTERY_ADC_PIN 34
#define AUDIO_ENABLE_PIN 4
#define AUDIO_DAC_PIN 26

#endif /* BOARD_PINS_H */
