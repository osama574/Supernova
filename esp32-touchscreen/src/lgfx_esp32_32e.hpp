#ifndef LGFX_ESP32_32E_HPP
#define LGFX_ESP32_32E_HPP

#include "board_pins.h"

#include <LovyanGFX.hpp>

class Panel_ST7796S_LCDWiki : public lgfx::Panel_ST7796 {
protected:
    const uint8_t * getInitCommands(uint8_t listno) const override
    {
        static constexpr uint8_t list0[] = {
            0x11, 0 + CMD_INIT_DELAY, 120,
            0x36, 1, 0x48,
            0x3A, 1, 0x55,

            0xF0, 1, 0xC3,
            0xF0, 1, 0x96,
            0xB4, 1, 0x01,
            0xB7, 1, 0xC6,
            0xC0, 2, 0x80, 0x45,
            0xC1, 1, 0x13,
            0xC2, 1, 0xA7,
            0xC5, 1, 0x20,
            0xE8, 8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33,

            0xE0, 14, 0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30,
                      0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31,
            0xE1, 14, 0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F,
                      0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32,

            0xF0, 1, 0x3C,
            0xF0, 1, 0x69,
            0x29, 0 + CMD_INIT_DELAY, 120,
            0xFF, 0xFF,
        };

        return listno == 0 ? list0 : nullptr;
    }
};

class LGFX : public lgfx::LGFX_Device {
    Panel_ST7796S_LCDWiki _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;
    lgfx::Touch_XPT2046 _touch;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus.config();
            cfg.spi_host = LCD_HOST_SPI;
            cfg.spi_mode = 0;
            cfg.freq_write = 27000000;
            cfg.freq_read = 8000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = 1;
            cfg.pin_sclk = LCD_PIN_SCLK;
            cfg.pin_mosi = LCD_PIN_MOSI;
            cfg.pin_miso = LCD_PIN_MISO;
            cfg.pin_dc = LCD_PIN_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        {
            auto cfg = _panel.config();
            cfg.pin_cs = LCD_PIN_CS;
            cfg.pin_rst = LCD_PIN_RST;
            cfg.pin_busy = -1;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel.config(cfg);
        }

        {
            auto cfg = _light.config();
            cfg.pin_bl = LCD_PIN_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }

        {
            auto cfg = _touch.config();
            cfg.spi_host = LCD_HOST_SPI;
            cfg.freq = 2500000;
            cfg.pin_sclk = LCD_PIN_SCLK;
            cfg.pin_mosi = LCD_PIN_MOSI;
            cfg.pin_miso = LCD_PIN_MISO;
            cfg.pin_cs = TOUCH_PIN_CS;
            cfg.pin_int = TOUCH_PIN_IRQ;
            cfg.bus_shared = true;

            /* First-pass calibration for landscape. Adjust if touch is mirrored or offset. */
            cfg.x_min = 300;
            cfg.x_max = 3900;
            cfg.y_min = 300;
            cfg.y_max = 3900;
            cfg.offset_rotation = 0;

            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }

        setPanel(&_panel);
    }
};

#endif /* LGFX_ESP32_32E_HPP */
