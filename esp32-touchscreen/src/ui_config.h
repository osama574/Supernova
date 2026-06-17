#ifndef UI_CONFIG_H
#define UI_CONFIG_H

/* Target board: LCDWiki/diymore 4.0inch ESP32-32E Display, SKU E32R40T. */
#define UI_TARGET_BOARD_NAME "4.0inch ESP32-32E Display"
#define UI_DISPLAY_WIDTH 480
#define UI_DISPLAY_HEIGHT 320

/* Hardware notes for the future ESP32 port. The PC simulator only uses width/height. */
#define UI_TARGET_LCD_DRIVER "ST7796S"
#define UI_TARGET_TOUCH_DRIVER "XPT2046"

#endif /* UI_CONFIG_H */
