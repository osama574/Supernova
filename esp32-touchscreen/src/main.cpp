#include <Arduino.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "board_pins.h"
#include "supernova_camera.h"
#include "supernova_mqtt.h"
#include "supernova_state.h"
#include "ui_boot.h"
#include "ui_config.h"

#if 0

static SPIClass test_spi(HSPI);
static const SPISettings test_spi_settings(1000000, MSBFIRST, SPI_MODE0);

static void test_select(void)
{
    digitalWrite(LCD_PIN_CS, LOW);
}

static void test_deselect(void)
{
    digitalWrite(LCD_PIN_CS, HIGH);
}

static void test_cmd(uint8_t cmd)
{
    digitalWrite(LCD_PIN_DC, LOW);
    test_select();
    test_spi.transfer(cmd);
    test_deselect();
}

static void test_data(uint8_t data)
{
    digitalWrite(LCD_PIN_DC, HIGH);
    test_select();
    test_spi.transfer(data);
    test_deselect();
}

static void test_data16(uint16_t data)
{
    test_spi.transfer(data >> 8);
    test_spi.transfer(data & 0xFF);
}

static void test_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    test_cmd(0x2A);
    digitalWrite(LCD_PIN_DC, HIGH);
    test_select();
    test_data16(x0);
    test_data16(x1);
    test_deselect();

    test_cmd(0x2B);
    digitalWrite(LCD_PIN_DC, HIGH);
    test_select();
    test_data16(y0);
    test_data16(y1);
    test_deselect();

    test_cmd(0x2C);
}

static void test_fill(uint16_t w, uint16_t h, uint16_t color)
{
    test_set_window(0, 0, w - 1, h - 1);
    digitalWrite(LCD_PIN_DC, HIGH);
    test_select();
    for(uint32_t i = 0; i < (uint32_t)w * h; ++i) {
        test_data16(color);
    }
    test_deselect();
}

static void test_init_st7796s(void)
{
    test_cmd(0x11);
    delay(120);

    test_cmd(0x36);
    test_data(0x48);
    test_cmd(0x3A);
    test_data(0x55);
    test_cmd(0xF0);
    test_data(0xC3);
    test_cmd(0xF0);
    test_data(0x96);
    test_cmd(0xB4);
    test_data(0x01);
    test_cmd(0xB7);
    test_data(0xC6);
    test_cmd(0xC0);
    test_data(0x80);
    test_data(0x45);
    test_cmd(0xC1);
    test_data(0x13);
    test_cmd(0xC2);
    test_data(0xA7);
    test_cmd(0xC5);
    test_data(0x20);
    test_cmd(0xE8);
    const uint8_t display_output[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    for(uint8_t value : display_output) {
        test_data(value);
    }
    test_cmd(0xE0);
    const uint8_t gamma_pos[] = {0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30,
                                 0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31};
    for(uint8_t value : gamma_pos) {
        test_data(value);
    }
    test_cmd(0xE1);
    const uint8_t gamma_neg[] = {0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F,
                                 0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32};
    for(uint8_t value : gamma_neg) {
        test_data(value);
    }
    test_cmd(0xF0);
    test_data(0x3C);
    test_cmd(0xF0);
    test_data(0x69);
    delay(120);
    test_cmd(0x29);
    delay(120);
}

void setup(void)
{
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("Starting ST7796S landscape diagnostic");

    pinMode(LCD_PIN_CS, OUTPUT);
    pinMode(LCD_PIN_DC, OUTPUT);
    pinMode(LCD_PIN_BL, OUTPUT);
    pinMode(TOUCH_PIN_CS, OUTPUT);
    pinMode(SD_PIN_CS, OUTPUT);

    digitalWrite(LCD_PIN_CS, HIGH);
    digitalWrite(TOUCH_PIN_CS, HIGH);
    digitalWrite(SD_PIN_CS, HIGH);
    digitalWrite(LCD_PIN_DC, HIGH);
    digitalWrite(LCD_PIN_BL, HIGH);

    test_spi.begin(LCD_PIN_SCLK, LCD_PIN_MISO, LCD_PIN_MOSI, LCD_PIN_CS);
    test_spi.beginTransaction(test_spi_settings);
    test_init_st7796s();
}

void loop(void)
{
    Serial.println("portrait red 320x480");
    test_cmd(0x36);
    test_data(0x48);
    test_fill(320, 480, 0xF800);
    delay(1500);

    Serial.println("landscape green 480x320 madctl 0x28");
    test_cmd(0x36);
    test_data(0x28);
    test_fill(480, 320, 0x07E0);
    delay(1500);

    Serial.println("landscape blue 480x320 madctl 0xE8");
    test_cmd(0x36);
    test_data(0xE8);
    test_fill(480, 320, 0x001F);
    delay(1500);
}

#else

SPIClass display_spi(HSPI);
static const uint32_t lcd_spi_frequency = 40000000;
static const uint16_t lvgl_buffer_rows = 80;
static const SPISettings lcd_spi_settings(lcd_spi_frequency, MSBFIRST, SPI_MODE0);
static const SPISettings touch_spi_settings(1000000, MSBFIRST, SPI_MODE0);

static lv_display_t * lvgl_display = NULL;
static lv_indev_t * lvgl_touch = NULL;
static lv_color_t * draw_buffer = NULL;
static uint16_t active_lvgl_buffer_rows = 0;
static uint32_t last_tick_ms = 0;
static lv_point_t last_touch_point = {UI_DISPLAY_WIDTH / 2, UI_DISPLAY_HEIGHT / 2};
static uint32_t last_touch_log_ms = 0;
static uint32_t touch_wake_ignore_until_ms = 0;

static void lcd_begin_write(void)
{
    display_spi.beginTransaction(lcd_spi_settings);
    digitalWrite(LCD_PIN_CS, LOW);
}

static void lcd_end_write(void)
{
    digitalWrite(LCD_PIN_CS, HIGH);
    display_spi.endTransaction();
}

static void lcd_write_command_inline(uint8_t cmd)
{
    digitalWrite(LCD_PIN_DC, LOW);
    display_spi.transfer(cmd);
    digitalWrite(LCD_PIN_DC, HIGH);
}

static void lcd_write_data_inline(uint8_t data)
{
    display_spi.transfer(data);
}

static void lcd_write_data16_inline(uint16_t data)
{
    display_spi.transfer(data >> 8);
    display_spi.transfer(data & 0xFF);
}

static void lcd_write_command(uint8_t cmd)
{
    lcd_begin_write();
    lcd_write_command_inline(cmd);
    lcd_end_write();
}

static void lcd_write_command_data(uint8_t cmd, const uint8_t * data, size_t length)
{
    lcd_begin_write();
    lcd_write_command_inline(cmd);
    for(size_t i = 0; i < length; ++i) {
        lcd_write_data_inline(data[i]);
    }
    lcd_end_write();
}

static void lcd_set_window_inline(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_write_command_inline(0x2A);
    lcd_write_data16_inline(x0);
    lcd_write_data16_inline(x1);

    lcd_write_command_inline(0x2B);
    lcd_write_data16_inline(y0);
    lcd_write_data16_inline(y1);

    lcd_write_command_inline(0x2C);
}

static void lcd_fill_screen(uint16_t color)
{
    lcd_begin_write();
    lcd_set_window_inline(0, 0, UI_DISPLAY_WIDTH - 1, UI_DISPLAY_HEIGHT - 1);
    for(uint32_t i = 0; i < (uint32_t)UI_DISPLAY_WIDTH * UI_DISPLAY_HEIGHT; ++i) {
        lcd_write_data16_inline(color);
    }
    lcd_end_write();
}

static void lcd_init_st7796s_lcdwiki(void)
{
    static const uint8_t madctl_portrait[] = {0x48};
    static const uint8_t pixel_format[] = {0x55};
    static const uint8_t command_set_c3[] = {0xC3};
    static const uint8_t command_set_96[] = {0x96};
    static const uint8_t inversion[] = {0x01};
    static const uint8_t entry_mode[] = {0xC6};
    static const uint8_t power_control_1[] = {0x80, 0x45};
    static const uint8_t power_control_2[] = {0x13};
    static const uint8_t power_control_3[] = {0xA7};
    static const uint8_t vcom_control[] = {0x20};
    static const uint8_t display_output[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    static const uint8_t gamma_pos[] = {0xD0, 0x08, 0x0F, 0x06, 0x06, 0x33, 0x30,
                                        0x33, 0x47, 0x17, 0x13, 0x13, 0x2B, 0x31};
    static const uint8_t gamma_neg[] = {0xD0, 0x0A, 0x11, 0x0B, 0x09, 0x07, 0x2F,
                                        0x33, 0x47, 0x38, 0x15, 0x16, 0x2C, 0x32};
    static const uint8_t command_set_3c[] = {0x3C};
    static const uint8_t command_set_69[] = {0x69};
    static const uint8_t madctl_landscape[] = {0x28};

    lcd_write_command(0x11);
    delay(120);

    lcd_write_command_data(0x36, madctl_portrait, sizeof(madctl_portrait));
    lcd_write_command_data(0x3A, pixel_format, sizeof(pixel_format));
    lcd_write_command_data(0xF0, command_set_c3, sizeof(command_set_c3));
    lcd_write_command_data(0xF0, command_set_96, sizeof(command_set_96));
    lcd_write_command_data(0xB4, inversion, sizeof(inversion));
    lcd_write_command_data(0xB7, entry_mode, sizeof(entry_mode));
    lcd_write_command_data(0xC0, power_control_1, sizeof(power_control_1));
    lcd_write_command_data(0xC1, power_control_2, sizeof(power_control_2));
    lcd_write_command_data(0xC2, power_control_3, sizeof(power_control_3));
    lcd_write_command_data(0xC5, vcom_control, sizeof(vcom_control));
    lcd_write_command_data(0xE8, display_output, sizeof(display_output));
    lcd_write_command_data(0xE0, gamma_pos, sizeof(gamma_pos));
    lcd_write_command_data(0xE1, gamma_neg, sizeof(gamma_neg));
    lcd_write_command_data(0xF0, command_set_3c, sizeof(command_set_3c));
    lcd_write_command_data(0xF0, command_set_69, sizeof(command_set_69));

    delay(120);
    lcd_write_command(0x29);
    delay(120);

    lcd_write_command_data(0x36, madctl_landscape, sizeof(madctl_landscape));
}

static void lvgl_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    const uint16_t width = area->x2 - area->x1 + 1;
    const uint16_t height = area->y2 - area->y1 + 1;
    const uint16_t * pixels = (const uint16_t *)px_map;

    lcd_begin_write();
    lcd_set_window_inline(area->x1, area->y1, area->x2, area->y2);
    display_spi.writePixels(pixels, (uint32_t)width * height * sizeof(uint16_t));
    lcd_end_write();

    lv_display_flush_ready(disp);
}

static uint16_t touch_read_channel_locked(uint8_t command)
{
    display_spi.transfer(command);
    delayMicroseconds(2);
    uint16_t value = display_spi.transfer(0x00) << 8;
    value |= display_spi.transfer(0x00);

    return value >> 3;
}

static bool touch_read_raw(uint16_t * raw_x, uint16_t * raw_y, uint16_t * z1, uint16_t * z2)
{
    digitalWrite(LCD_PIN_CS, HIGH);
    display_spi.beginTransaction(touch_spi_settings);
    digitalWrite(TOUCH_PIN_CS, LOW);
    delayMicroseconds(2);

    uint32_t raw_x_sum = 0;
    uint32_t raw_y_sum = 0;

    *z1 = touch_read_channel_locked(0xB0);
    *z2 = touch_read_channel_locked(0xC0);

    for(uint8_t i = 0; i < 4; ++i) {
        raw_x_sum += touch_read_channel_locked(0xD0);
        raw_y_sum += touch_read_channel_locked(0x90);
    }

    digitalWrite(TOUCH_PIN_CS, HIGH);
    display_spi.endTransaction();

    *raw_x = raw_x_sum / 4;
    *raw_y = raw_y_sum / 4;

    const bool irq_pressed = digitalRead(TOUCH_PIN_IRQ) == LOW;
    const bool raw_valid = *raw_x > 150 && *raw_x < 4050 && *raw_y > 150 && *raw_y < 4050;
    const bool pressure_valid = *z1 > 80 && *z1 < 4050;

    return raw_valid && (irq_pressed || pressure_valid);
}

static int16_t map_clamped(uint16_t value, uint16_t in_min, uint16_t in_max, int16_t out_min, int16_t out_max)
{
    if(in_min < in_max) {
        if(value < in_min) {
            value = in_min;
        }
        if(value > in_max) {
            value = in_max;
        }
    }
    else {
        if(value > in_min) {
            value = in_min;
        }
        if(value < in_max) {
            value = in_max;
        }
    }

    return (int32_t)(value - in_min) * (out_max - out_min) / (int32_t)(in_max - in_min) + out_min;
}

static void lvgl_touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    LV_UNUSED(indev);

    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    uint16_t z1 = 0;
    uint16_t z2 = 0;

    if(!touch_read_raw(&raw_x, &raw_y, &z1, &z2)) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point = last_touch_point;
        return;
    }

    const uint32_t now = millis();
    if(supernova_screen_sleeping()) {
        supernova_screen_wake();
        touch_wake_ignore_until_ms = now + 500;
        data->state = LV_INDEV_STATE_RELEASED;
        data->point = last_touch_point;
        return;
    }

    if((int32_t)(now - touch_wake_ignore_until_ms) < 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point = last_touch_point;
        return;
    }

    supernova_screen_touch_activity();

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = map_clamped(raw_y, 3900, 300, 0, UI_DISPLAY_WIDTH - 1);
    data->point.y = map_clamped(raw_x, 3900, 300, 0, UI_DISPLAY_HEIGHT - 1);
    last_touch_point = data->point;

    if(now - last_touch_log_ms > 1500) {
        last_touch_log_ms = now;
        Serial.printf("Touch irq=%d raw_x=%u raw_y=%u z1=%u z2=%u -> x=%d y=%d\n",
                      digitalRead(TOUCH_PIN_IRQ),
                      raw_x,
                      raw_y,
                      z1,
                      z2,
                      data->point.x,
                      data->point.y);
    }
}

static void setup_pins(void)
{
    pinMode(LCD_PIN_CS, OUTPUT);
    pinMode(LCD_PIN_DC, OUTPUT);
    pinMode(LCD_PIN_BL, OUTPUT);
    pinMode(TOUCH_PIN_CS, OUTPUT);
    pinMode(TOUCH_PIN_IRQ, INPUT);
    pinMode(SD_PIN_CS, OUTPUT);

    digitalWrite(LCD_PIN_CS, HIGH);
    digitalWrite(TOUCH_PIN_CS, HIGH);
    digitalWrite(SD_PIN_CS, HIGH);
    digitalWrite(LCD_PIN_DC, HIGH);
    digitalWrite(LCD_PIN_BL, HIGH);
}

static void setup_lvgl(void)
{
    lv_init();

    static const uint16_t candidate_rows[] = {lvgl_buffer_rows, 60, 40, 20};
    for(uint8_t i = 0; i < (uint8_t)(sizeof(candidate_rows) / sizeof(candidate_rows[0])); ++i) {
        const size_t buffer_size = (size_t)UI_DISPLAY_WIDTH * candidate_rows[i] * sizeof(lv_color_t);
        draw_buffer = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if(draw_buffer != NULL) {
            active_lvgl_buffer_rows = candidate_rows[i];
            Serial.printf("LVGL draw buffer: %u rows, %u bytes\n",
                          active_lvgl_buffer_rows,
                          (unsigned)buffer_size);
            break;
        }
    }

    if(draw_buffer == NULL) {
        Serial.println("LVGL draw buffer allocation failed");
        return;
    }

    lvgl_display = lv_display_create(UI_DISPLAY_WIDTH, UI_DISPLAY_HEIGHT);
    lv_display_set_color_format(lvgl_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvgl_display, lvgl_flush_cb);
    lv_display_set_buffers(lvgl_display,
                           draw_buffer,
                           NULL,
                           (uint32_t)UI_DISPLAY_WIDTH * active_lvgl_buffer_rows * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lvgl_touch = lv_indev_create();
    lv_indev_set_type(lvgl_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(lvgl_touch, lvgl_display);
    lv_indev_set_read_cb(lvgl_touch, lvgl_touch_read_cb);
}

static void show_lvgl_simple_test(void)
{
    lv_obj_t * screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t * label = lv_label_create(screen);
    lv_label_set_text(label, "LVGL OK");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 26);

    static const lv_color_t colors[] = {
        LV_COLOR_MAKE(0xFF, 0x00, 0x00),
        LV_COLOR_MAKE(0x00, 0xFF, 0x00),
        LV_COLOR_MAKE(0x00, 0x00, 0xFF),
        LV_COLOR_MAKE(0xFF, 0xFF, 0x00),
    };

    for(uint8_t i = 0; i < 4; ++i) {
        lv_obj_t * bar = lv_obj_create(screen);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 420, 38);
        lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 82 + (int32_t)i * 52);
        lv_obj_set_style_bg_color(bar, colors[i], 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    }

    lv_screen_load(screen);
}

void setup(void)
{
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("Starting 4.0inch ESP32-32E Smart Display");

    setup_pins();
    display_spi.begin(LCD_PIN_SCLK, LCD_PIN_MISO, LCD_PIN_MOSI, LCD_PIN_CS);
    lcd_init_st7796s_lcdwiki();
    lcd_fill_screen(0x0000);

    setup_lvgl();
    last_tick_ms = millis();
    supernova_state_init();
    supernova_camera_init();
    supernova_mqtt_init();

#ifdef LVGL_SIMPLE_TEST
    show_lvgl_simple_test();
#else
    ui_show_boot_screen();
#endif
}

void loop(void)
{
    uint32_t now = millis();
    lv_tick_inc(now - last_tick_ms);
    last_tick_ms = now;

    supernova_state_poll();
    supernova_camera_poll();
    supernova_mqtt_poll();
    lv_timer_handler();
    delay(1);
}

#endif
