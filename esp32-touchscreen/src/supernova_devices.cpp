#include "supernova_devices.h"

#include "board_pins.h"

#include <stdio.h>
#include <string.h>

#if defined(ARDUINO)
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>

extern SPIClass display_spi;
static SPIClass sd_spi(VSPI);
#endif

static const uint32_t sd_spi_frequency = 10000000;
static const bool rgb_led_enabled = false;
static const uint8_t backlight_pwm_channel = 0;
static const uint16_t backlight_pwm_frequency = 5000;
static const uint8_t backlight_pwm_resolution = 8;
static const uint8_t servo_pan_pwm_channel = 6;
static const uint8_t servo_tilt_pwm_channel = 7;
static const uint16_t servo_pwm_frequency = 50;
static const uint8_t servo_pwm_resolution = 16;
static const uint16_t servo_min_pulse_us = 500;
static const uint16_t servo_max_pulse_us = 2500;
static const uint16_t servo_period_us = 20000;
static const int16_t servo_safe_min_angle = 5;
static const int16_t servo_safe_max_angle = 265;

static uint8_t current_volume = 55;
static uint8_t current_brightness = 85;
static uint8_t current_led_mode = 1;
static int16_t servo_pan = 135;
static int16_t servo_tilt = 135;
static bool test_tone_active = false;
static uint8_t audio_phase = 0;
static uint32_t next_audio_sample_us = 0;
static uint8_t rgb_music_step = 0;
static uint32_t next_rgb_music_ms = 0;
static bool sd_ready = false;
static bool sd_spi_started = false;
static bool music_scanned = false;
static supernova_music_scan_status_t music_scan_status = SUPERNOVA_MUSIC_NOT_SCANNED;
static uint8_t music_file_count = 0;
static uint8_t ignored_music_file_count = 0;
static const uint8_t max_music_files = 120;
static char music_paths[max_music_files][72];
static char music_names[max_music_files][48];

#if defined(ARDUINO)
static const i2s_port_t audio_i2s_port = I2S_NUM_0;
static const uint16_t led_pixel_count = 113;
static const uint8_t led_ring_brightness = 5;
static const uint8_t led_ring_sizes[] = {32, 24, 20, 16, 12, 8, 1};
static Adafruit_NeoPixel led_strip(led_pixel_count, SUPERNOVA_LED_DATA_PIN, NEO_GRB + NEO_KHZ800);
static TaskHandle_t music_scan_task_handle = NULL;
static TaskHandle_t music_task_handle = NULL;
static volatile bool music_scan_running = false;
static volatile bool music_task_running = false;
static volatile bool music_stop_requested = false;
static bool audio_i2s_ready = false;
static bool addressable_led_ready = false;
static uint32_t last_led_update_ms = 0;
static uint16_t led_animation_step = 0;
#endif

static const uint8_t rgb_music_colors[][3] = {
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 1, 0},
    {0, 1, 1},
    {1, 0, 1},
    {1, 1, 1},
};

static const uint8_t sine_32[] = {
    128, 153, 177, 199, 218, 234, 245, 253,
    255, 253, 245, 234, 218, 199, 177, 153,
    128, 103, 79, 57, 38, 22, 11, 3,
    1, 3, 11, 22, 38, 57, 79, 103,
};

static int16_t clamp_angle(int16_t value)
{
    if(value < servo_safe_min_angle) {
        return servo_safe_min_angle;
    }
    if(value > servo_safe_max_angle) {
        return servo_safe_max_angle;
    }
    return value;
}

#if defined(ARDUINO)
static uint32_t servo_duty_for_angle(int16_t angle)
{
    const uint32_t pulse_us =
        servo_min_pulse_us +
        ((uint32_t)clamp_angle(angle) * (servo_max_pulse_us - servo_min_pulse_us)) / 270UL;
    const uint32_t max_duty = (1UL << servo_pwm_resolution) - 1UL;
    return (pulse_us * max_duty) / servo_period_us;
}

static void write_pan_servo(void)
{
    ledcWrite(servo_pan_pwm_channel, servo_duty_for_angle(servo_pan));
}

static void write_tilt_servo(void)
{
    ledcWrite(servo_tilt_pwm_channel, servo_duty_for_angle(servo_tilt));
}
#endif

static void set_rgb_led(uint8_t red, uint8_t green, uint8_t blue)
{
#if defined(ARDUINO)
    if(!rgb_led_enabled) {
        digitalWrite(RGB_LED_RED, HIGH);
        digitalWrite(RGB_LED_GREEN, HIGH);
        digitalWrite(RGB_LED_BLUE, HIGH);
        return;
    }

    digitalWrite(RGB_LED_RED, red ? LOW : HIGH);
    digitalWrite(RGB_LED_GREEN, green ? LOW : HIGH);
    digitalWrite(RGB_LED_BLUE, blue ? LOW : HIGH);
#else
    (void)red;
    (void)green;
    (void)blue;
#endif
}

#if defined(ARDUINO)
static uint32_t led_wheel(uint8_t position)
{
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    if(position < 85) {
        red = 255 - position * 3;
        green = position * 3;
    }
    else if(position < 170) {
        position -= 85;
        green = 255 - position * 3;
        blue = position * 3;
    }
    else {
        position -= 170;
        blue = 255 - position * 3;
        red = position * 3;
    }

    return led_strip.Color(red, green, blue);
}

static uint32_t ring_color(uint8_t ring)
{
    switch(ring) {
        case 0:
            return led_strip.Color(255, 0, 0);
        case 1:
            return led_strip.Color(255, 80, 0);
        case 2:
            return led_strip.Color(255, 255, 0);
        case 3:
            return led_strip.Color(0, 255, 0);
        case 4:
            return led_strip.Color(0, 0, 255);
        case 5:
            return led_strip.Color(120, 0, 255);
        case 6:
            return led_strip.Color(255, 255, 255);
        default:
            return led_strip.Color(0, 0, 0);
    }
}

static void led_fill(uint32_t color)
{
    led_strip.fill(color, 0, led_pixel_count);
}

static bool init_addressable_led(void)
{
    if(addressable_led_ready) {
        return true;
    }

    led_strip.begin();
    led_strip.setBrightness(led_ring_brightness);
    led_strip.clear();
    led_strip.show();
    addressable_led_ready = true;
    Serial.printf("NeoPixel LED ring ready: %u pixels on GPIO%d brightness=%u\n",
                  led_pixel_count,
                  SUPERNOVA_LED_DATA_PIN,
                  led_ring_brightness);
    return true;
}

static void led_show(void)
{
    if(!init_addressable_led()) {
        return;
    }
    led_strip.show();
}

static void led_render_mode(uint32_t now_ms, bool force)
{
    if(!force && now_ms - last_led_update_ms < 50UL) {
        return;
    }
    last_led_update_ms = now_ms;

    if(!init_addressable_led()) {
        return;
    }

    switch(current_led_mode) {
        case 0:
            led_fill(led_strip.Color(0, 0, 0));
            break;
        case 1: {
            led_strip.clear();
            uint16_t start_index = 0;
            uint16_t step = led_animation_step % led_pixel_count;
            for(uint8_t ring = 0; ring < (uint8_t)(sizeof(led_ring_sizes) / sizeof(led_ring_sizes[0])); ++ring) {
                const uint8_t leds_in_ring = led_ring_sizes[ring];
                if(step < leds_in_ring) {
                    led_strip.setPixelColor(start_index + step, ring_color(ring));
                    break;
                }
                step -= leds_in_ring;
                start_index += leds_in_ring;
            }
            led_animation_step++;
            break;
        }
        case 2:
            for(uint16_t i = 0; i < led_pixel_count; ++i) {
                const uint8_t pos = (uint8_t)((i * 256UL / led_pixel_count + led_animation_step) & 0xFFU);
                led_strip.setPixelColor(i, led_wheel(pos));
            }
            led_animation_step += 3;
            break;
        case 3: {
            const bool on = ((now_ms / 180UL) & 1U) == 0;
            led_fill(on ? led_strip.Color(255, 0, 32) : led_strip.Color(20, 0, 0));
            break;
        }
        default:
            led_fill(led_strip.Color(0, 0, 0));
            break;
    }

    led_show();
}

static bool init_sd_card(void)
{
    if(sd_ready) {
        return true;
    }

    digitalWrite(LCD_PIN_CS, HIGH);
    digitalWrite(TOUCH_PIN_CS, HIGH);
    digitalWrite(SD_PIN_CS, HIGH);
    if(!sd_spi_started) {
        sd_spi.begin(SD_PIN_SCLK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
        sd_spi_started = true;
    }

    sd_ready = SD.begin(SD_PIN_CS, sd_spi, sd_spi_frequency);
    Serial.printf("SD card %s\n", sd_ready ? "ready" : "failed");
    return sd_ready;
}

static bool text_ends_with_wav(const char * text)
{
    const size_t len = strlen(text);
    if(len < 4) {
        return false;
    }

    const char * ext = text + len - 4;
    return (ext[0] == '.' &&
            (ext[1] == 'w' || ext[1] == 'W') &&
            (ext[2] == 'a' || ext[2] == 'A') &&
            (ext[3] == 'v' || ext[3] == 'V'));
}

static const char * base_name(const char * path)
{
    const char * slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static void scan_music_files(void)
{
    if(music_scanned) {
        return;
    }

#if defined(ARDUINO)
    if(music_scan_running && xTaskGetCurrentTaskHandle() != music_scan_task_handle) {
        return;
    }
    music_scan_running = true;
#endif

    music_scanned = true;
    music_file_count = 0;
    ignored_music_file_count = 0;
    music_scan_status = SUPERNOVA_MUSIC_NOT_SCANNED;
    if(!init_sd_card()) {
        music_scan_status = SUPERNOVA_MUSIC_SD_MOUNT_FAILED;
#if defined(ARDUINO)
        music_scan_running = false;
#endif
        return;
    }

    File dir = SD.open("/music");
    if(!dir || !dir.isDirectory()) {
        if(dir) {
            dir.close();
        }
        dir = SD.open("/Music");
    }
    if(!dir || !dir.isDirectory()) {
        if(dir) {
            dir.close();
        }
        dir = SD.open("/MUSIC");
    }
    if(!dir || !dir.isDirectory()) {
        Serial.println("SD /music folder not found");
        music_scan_status = SUPERNOVA_MUSIC_FOLDER_MISSING;
#if defined(ARDUINO)
        music_scan_running = false;
#endif
        return;
    }

    while(true) {
        File entry = dir.openNextFile();
        if(!entry) {
            break;
        }

        if(!entry.isDirectory()) {
            const char * entry_name = entry.name();
            if(text_ends_with_wav(entry_name)) {
                if(music_file_count < max_music_files) {
                    if(entry_name[0] == '/') {
                        snprintf(music_paths[music_file_count], sizeof(music_paths[music_file_count]), "%s", entry_name);
                    }
                    else {
                        snprintf(music_paths[music_file_count], sizeof(music_paths[music_file_count]), "/music/%s", entry_name);
                    }
                    snprintf(music_names[music_file_count], sizeof(music_names[music_file_count]), "%s",
                             base_name(music_paths[music_file_count]));
                    Serial.printf("Found WAV: %s\n", music_paths[music_file_count]);
                    music_file_count++;
                }
                else if(ignored_music_file_count < UINT8_MAX) {
                    ignored_music_file_count++;
                }
            }
            else {
                if(ignored_music_file_count < UINT8_MAX) {
                    ignored_music_file_count++;
                }
            }
        }

        entry.close();
    }

    dir.close();
    music_scan_status = music_file_count > 0 ? SUPERNOVA_MUSIC_READY : SUPERNOVA_MUSIC_NO_WAV_FILES;
    Serial.printf("Music WAV count: %u\n", music_file_count);
#if defined(ARDUINO)
    music_scan_running = false;
#endif
}

static void music_scan_task(void * parameter)
{
    (void)parameter;
    music_scan_task_handle = xTaskGetCurrentTaskHandle();
    scan_music_files();
    music_scan_task_handle = NULL;
    music_scan_running = false;
    vTaskDelete(NULL);
}

static uint16_t read_u16_le(File & file)
{
    uint8_t bytes[2];
    if(file.read(bytes, sizeof(bytes)) != sizeof(bytes)) {
        return 0;
    }
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_u32_le(File & file)
{
    uint8_t bytes[4];
    if(file.read(bytes, sizeof(bytes)) != sizeof(bytes)) {
        return 0;
    }
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

typedef struct {
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t sample_rate;
    uint32_t data_bytes;
} wav_info_t;

static void seek_past_chunk(File & file, uint32_t chunk_size)
{
    file.seek(file.position() + chunk_size + (chunk_size & 1U));
}

static bool read_wav_info(File & file, wav_info_t * info)
{
    char id[5] = {};
    if(file.read((uint8_t *)id, 4) != 4 || strncmp(id, "RIFF", 4) != 0) {
        return false;
    }
    (void)read_u32_le(file);
    if(file.read((uint8_t *)id, 4) != 4 || strncmp(id, "WAVE", 4) != 0) {
        return false;
    }

    bool has_fmt = false;
    while(file.available()) {
        if(file.read((uint8_t *)id, 4) != 4) {
            return false;
        }
        uint32_t chunk_size = read_u32_le(file);

        if(strncmp(id, "fmt ", 4) == 0) {
            uint16_t audio_format = read_u16_le(file);
            info->channels = read_u16_le(file);
            info->sample_rate = read_u32_le(file);
            (void)read_u32_le(file);
            (void)read_u16_le(file);
            info->bits_per_sample = read_u16_le(file);
            if(chunk_size > 16) {
                seek_past_chunk(file, chunk_size - 16);
            }
            has_fmt = audio_format == 1 && (info->channels == 1 || info->channels == 2) &&
                      (info->bits_per_sample == 8 || info->bits_per_sample == 16);
        }
        else if(strncmp(id, "data", 4) == 0) {
            info->data_bytes = chunk_size;
            return has_fmt;
        }
        else {
            seek_past_chunk(file, chunk_size);
        }
    }

    return false;
}

static void stop_i2s_dac(void)
{
    if(audio_i2s_ready) {
        i2s_zero_dma_buffer(audio_i2s_port);
        i2s_driver_uninstall(audio_i2s_port);
        audio_i2s_ready = false;
    }
}

static bool start_i2s_dac(uint32_t sample_rate)
{
    stop_i2s_dac();

    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    config.sample_rate = sample_rate;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    config.intr_alloc_flags = 0;
    config.dma_buf_count = 8;
    config.dma_buf_len = 512;
    config.use_apll = true;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;

    if(i2s_driver_install(audio_i2s_port, &config, 0, NULL) != ESP_OK) {
        Serial.println("I2S DAC driver install failed");
        return false;
    }
    if(i2s_set_pin(audio_i2s_port, NULL) != ESP_OK) {
        i2s_driver_uninstall(audio_i2s_port);
        Serial.println("I2S DAC pin setup failed");
        return false;
    }
    /* Audio is wired to DAC channel 2 on GPIO26. Keep GPIO25 free for pan PWM. */
    if(i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN) != ESP_OK) {
        i2s_driver_uninstall(audio_i2s_port);
        Serial.println("I2S DAC mode failed");
        return false;
    }
    if(i2s_set_sample_rates(audio_i2s_port, sample_rate) != ESP_OK) {
        i2s_driver_uninstall(audio_i2s_port);
        Serial.println("I2S DAC sample rate setup failed");
        return false;
    }

    i2s_zero_dma_buffer(audio_i2s_port);
    audio_i2s_ready = true;
    return true;
}

static uint8_t pcm_to_dac_value(int32_t mixed, uint16_t bits_per_sample)
{
    int32_t dac_value = 128;
    if(bits_per_sample == 16) {
        dac_value += ((mixed * (int32_t)current_volume) / 100) / 256;
    }
    else {
        dac_value += (mixed * (int32_t)current_volume) / 100;
    }

    if(dac_value < 0) {
        return 0;
    }
    if(dac_value > 255) {
        return 255;
    }
    return (uint8_t)dac_value;
}

static void stop_wav_playback(void)
{
    if(music_task_running || music_task_handle != NULL) {
        music_stop_requested = true;
        for(uint8_t i = 0; i < 80 && music_task_running; ++i) {
            delay(10);
        }
    }
    if(!music_task_running) {
        music_task_handle = NULL;
        stop_i2s_dac();
        dacWrite(AUDIO_DAC_PIN, 128);
        digitalWrite(AUDIO_ENABLE_PIN, HIGH);
    }
}

static void music_playback_task(void * parameter)
{
    const uint8_t index = (uint8_t)(uintptr_t)parameter;
    music_task_running = true;

    File file = SD.open(music_paths[index], FILE_READ);
    if(!file) {
        Serial.printf("Could not open WAV: %s\n", music_paths[index]);
        music_task_running = false;
        music_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    wav_info_t info = {};
    if(!read_wav_info(file, &info)) {
        Serial.printf("Unsupported WAV: %s\n", music_paths[index]);
        file.close();
        music_task_running = false;
        music_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    const uint32_t frame_bytes = (uint32_t)info.channels * (info.bits_per_sample / 8U);
    if(frame_bytes == 0 || info.sample_rate == 0 || !start_i2s_dac(info.sample_rate)) {
        file.close();
        music_task_running = false;
        music_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    Serial.printf("Playing WAV: %s %luHz %uch %ubit\n",
                  music_names[index],
                  info.sample_rate,
                  info.channels,
                  info.bits_per_sample);

    digitalWrite(AUDIO_ENABLE_PIN, LOW);
    uint32_t frames_left = info.data_bytes / frame_bytes;
    uint32_t next_rgb_ms = 0;
    uint8_t rgb_step = 0;

    static const size_t max_frames_per_chunk = 512;
    uint8_t input_buffer[max_frames_per_chunk * 4];
    uint16_t output_buffer[max_frames_per_chunk * 2];

    while(frames_left > 0 && !music_stop_requested) {
        size_t frames_this_chunk = frames_left > max_frames_per_chunk ? max_frames_per_chunk : (size_t)frames_left;
        const size_t bytes_to_read = frames_this_chunk * frame_bytes;
        const size_t bytes_read = file.read(input_buffer, bytes_to_read);
        if(bytes_read < frame_bytes) {
            break;
        }
        frames_this_chunk = bytes_read / frame_bytes;

        size_t output_count = 0;
        for(size_t frame = 0; frame < frames_this_chunk; ++frame) {
            const uint8_t * data = input_buffer + frame * frame_bytes;
            int32_t mixed = 0;

            for(uint16_t ch = 0; ch < info.channels; ++ch) {
                if(info.bits_per_sample == 16) {
                    const uint8_t * sample = data + ch * 2;
                    mixed += (int16_t)((uint16_t)sample[0] | ((uint16_t)sample[1] << 8));
                }
                else {
                    mixed += (int32_t)data[ch] - 128;
                }
            }
            mixed /= info.channels;

            const uint16_t dac_sample = (uint16_t)pcm_to_dac_value(mixed, info.bits_per_sample) << 8;
            output_buffer[output_count++] = dac_sample;
            output_buffer[output_count++] = dac_sample;
        }

        size_t bytes_written = 0;
        i2s_write(audio_i2s_port, output_buffer, output_count * sizeof(output_buffer[0]), &bytes_written, portMAX_DELAY);
        frames_left -= frames_this_chunk;

        const uint32_t now_ms = millis();
        if(now_ms >= next_rgb_ms) {
            const uint8_t * color = rgb_music_colors[rgb_step];
            set_rgb_led(color[0], color[1], color[2]);
            rgb_step = (rgb_step + 1) % (sizeof(rgb_music_colors) / sizeof(rgb_music_colors[0]));
            next_rgb_ms = now_ms + 150;
        }
    }

    file.close();
    stop_i2s_dac();
    dacWrite(AUDIO_DAC_PIN, 128);
    digitalWrite(AUDIO_ENABLE_PIN, HIGH);
    set_rgb_led(0, 0, 0);
    music_task_running = false;
    music_task_handle = NULL;
    vTaskDelete(NULL);
}
#endif

void supernova_devices_init(void)
{
#if defined(ARDUINO)
    ledcSetup(backlight_pwm_channel, backlight_pwm_frequency, backlight_pwm_resolution);
    ledcAttachPin(LCD_PIN_BL, backlight_pwm_channel);
    supernova_devices_set_brightness(current_brightness);

    pinMode(AUDIO_ENABLE_PIN, OUTPUT);
    digitalWrite(AUDIO_ENABLE_PIN, HIGH);
    dacWrite(AUDIO_DAC_PIN, 128);

    pinMode(RGB_LED_RED, OUTPUT);
    pinMode(RGB_LED_GREEN, OUTPUT);
    pinMode(RGB_LED_BLUE, OUTPUT);
    set_rgb_led(0, 0, 0);

    pinMode(SUPERNOVA_LED_DATA_PIN, OUTPUT);

    const uint32_t pan_pwm_frequency =
        ledcSetup(servo_pan_pwm_channel, servo_pwm_frequency, servo_pwm_resolution);
    const uint32_t tilt_pwm_frequency =
        ledcSetup(servo_tilt_pwm_channel, servo_pwm_frequency, servo_pwm_resolution);
    ledcAttachPin(SUPERNOVA_SERVO_PAN_PIN, servo_pan_pwm_channel);
    ledcAttachPin(SUPERNOVA_SERVO_TILT_PIN, servo_tilt_pwm_channel);
    write_pan_servo();
    write_tilt_servo();

    Serial.printf("Servo PWM: pan=%luHz tilt=%luHz center=%d degrees\n",
                  pan_pwm_frequency,
                  tilt_pwm_frequency,
                  servo_pan);
    Serial.printf("Supernova pins: I2S BCLK=%d LRCLK=%d DOUT=%d LED=%d ServoPan=%d ServoTilt=%d\n",
                  SUPERNOVA_I2S_BCLK_PIN,
                  SUPERNOVA_I2S_LRCLK_PIN,
                  SUPERNOVA_I2S_DOUT_PIN,
                  SUPERNOVA_LED_DATA_PIN,
                  SUPERNOVA_SERVO_PAN_PIN,
                  SUPERNOVA_SERVO_TILT_PIN);
#endif
}

static uint8_t scaled_sine_sample(uint8_t phase)
{
    const uint16_t amplitude = (uint16_t)current_volume * 96U / 100U;
    int16_t centered = (int16_t)sine_32[phase & 31U] - 128;
    return (uint8_t)(128 + (centered * (int16_t)amplitude) / 127);
}

static void play_dac_tone(uint16_t frequency, uint16_t duration_ms)
{
#if defined(ARDUINO)
    if(frequency == 0 || duration_ms == 0 || current_volume == 0) {
        return;
    }

    const uint32_t sample_delay_us = 1000000UL / ((uint32_t)frequency * 32UL);
    const uint32_t sample_count = (uint32_t)frequency * duration_ms * 32UL / 1000UL;
    for(uint32_t i = 0; i < sample_count; ++i) {
        dacWrite(AUDIO_DAC_PIN, scaled_sine_sample((uint8_t)i));
        delayMicroseconds(sample_delay_us);
    }
#else
    (void)frequency;
    (void)duration_ms;
#endif
}

void supernova_devices_play_test_tone(void)
{
#if defined(ARDUINO)
    Serial.println("Playing onboard speaker test tone");
    digitalWrite(AUDIO_ENABLE_PIN, LOW);
    delay(15);
    play_dac_tone(523, 160);
    delay(35);
    play_dac_tone(659, 160);
    delay(35);
    play_dac_tone(784, 220);
    dacWrite(AUDIO_DAC_PIN, 128);
    delay(20);
    digitalWrite(AUDIO_ENABLE_PIN, HIGH);
#endif
}

void supernova_devices_start_test_tone(void)
{
#if defined(ARDUINO)
    stop_wav_playback();
    Serial.println("Starting continuous speaker test tone");
    test_tone_active = true;
    audio_phase = 0;
    next_audio_sample_us = micros();
    rgb_music_step = 0;
    next_rgb_music_ms = 0;
    digitalWrite(AUDIO_ENABLE_PIN, LOW);
#endif
}

void supernova_devices_stop_test_tone(void)
{
#if defined(ARDUINO)
    stop_wav_playback();
    if(test_tone_active) {
        Serial.println("Stopping speaker test tone");
    }
    test_tone_active = false;
    dacWrite(AUDIO_DAC_PIN, 128);
    digitalWrite(AUDIO_ENABLE_PIN, HIGH);
    set_rgb_led(0, 0, 0);
#endif
}

void supernova_devices_audio_poll(void)
{
#if defined(ARDUINO)
    if(!test_tone_active) {
        return;
    }

    const uint32_t now_us = micros();
    if((int32_t)(now_us - next_audio_sample_us) < 0) {
        return;
    }

    if(current_volume == 0) {
        dacWrite(AUDIO_DAC_PIN, 128);
    }
    else {
        dacWrite(AUDIO_DAC_PIN, scaled_sine_sample(audio_phase++));
    }
    next_audio_sample_us += 52;

    const uint32_t now_ms = millis();
    if(now_ms >= next_rgb_music_ms) {
        const uint8_t * color = rgb_music_colors[rgb_music_step];
        set_rgb_led(color[0], color[1], color[2]);
        rgb_music_step = (rgb_music_step + 1) % (sizeof(rgb_music_colors) / sizeof(rgb_music_colors[0]));
        next_rgb_music_ms = now_ms + 150;
    }
#endif
}

void supernova_devices_led_poll(void)
{
#if defined(ARDUINO)
    led_render_mode(millis(), false);
#endif
}

void supernova_devices_music_preload_async(void)
{
#if defined(ARDUINO)
    if(music_scanned || music_scan_running || music_scan_task_handle != NULL) {
        return;
    }

    music_scan_running = true;
    if(xTaskCreatePinnedToCore(music_scan_task,
                               "music_scan",
                               4096,
                               NULL,
                               1,
                               &music_scan_task_handle,
                               0) != pdPASS) {
        music_scan_task_handle = NULL;
        music_scan_running = false;
        music_scanned = true;
        Serial.println("Music preload task start failed");
    }
#endif
}

bool supernova_devices_music_preload_done(void)
{
#if defined(ARDUINO)
    return music_scanned && !music_scan_running && music_scan_task_handle == NULL;
#else
    return true;
#endif
}

uint8_t supernova_devices_music_count(void)
{
#if defined(ARDUINO)
    scan_music_files();
    return music_file_count;
#else
    return 0;
#endif
}

const char * supernova_devices_music_name(uint8_t index)
{
#if defined(ARDUINO)
    scan_music_files();
    if(index < music_file_count) {
        return music_names[index];
    }
#else
    (void)index;
#endif
    return "";
}

supernova_music_scan_status_t supernova_devices_music_status(void)
{
#if defined(ARDUINO)
    scan_music_files();
#endif
    return music_scan_status;
}

uint8_t supernova_devices_music_ignored_count(void)
{
#if defined(ARDUINO)
    scan_music_files();
#endif
    return ignored_music_file_count;
}

void supernova_devices_music_rescan(void)
{
#if defined(ARDUINO)
    stop_wav_playback();
    music_scanned = false;
    music_file_count = 0;
    ignored_music_file_count = 0;
    music_scan_status = SUPERNOVA_MUSIC_NOT_SCANNED;
    scan_music_files();
#endif
}

bool supernova_devices_music_active(void)
{
#if defined(ARDUINO)
    return music_task_running || test_tone_active;
#else
    return false;
#endif
}

bool supernova_devices_play_music_preview(uint8_t index)
{
#if defined(ARDUINO)
    scan_music_files();
    if(index >= music_file_count) {
        return false;
    }

    supernova_devices_stop_test_tone();

    music_stop_requested = false;
    music_task_running = true;
    if(xTaskCreatePinnedToCore(music_playback_task,
                               "music_wav",
                               8192,
                               (void *)(uintptr_t)index,
                               2,
                               &music_task_handle,
                               0) != pdPASS) {
        music_task_handle = NULL;
        music_task_running = false;
        Serial.println("Music playback task start failed");
        return false;
    }
    return true;
#else
    (void)index;
    return false;
#endif
}

void supernova_devices_set_brightness(uint8_t brightness)
{
    current_brightness = brightness > 100 ? 100 : brightness;
#if defined(ARDUINO)
    const uint32_t max_duty = (1UL << backlight_pwm_resolution) - 1UL;
    const uint32_t duty = (uint32_t)current_brightness * max_duty / 100UL;
    ledcWrite(backlight_pwm_channel, duty);
    Serial.printf("LCD backlight brightness set to %u%% duty=%lu\n", current_brightness, duty);
#endif
}

void supernova_devices_set_volume(uint8_t volume)
{
    current_volume = volume > 100 ? 100 : volume;
#if defined(ARDUINO)
    Serial.printf("Audio volume set to %u%%\n", current_volume);
#endif
}

void supernova_devices_set_led_mode(uint8_t mode)
{
    current_led_mode = mode;
#if defined(ARDUINO)
    led_animation_step = 0;
    last_led_update_ms = 0;
    led_render_mode(millis(), true);
    Serial.printf("LED mode set to %u\n", current_led_mode);
#endif
}

void supernova_devices_servo_nudge(int8_t pan_delta, int8_t tilt_delta)
{
#if defined(ARDUINO)
    if(pan_delta != 0) {
        const int16_t next_pan = clamp_angle(servo_pan + pan_delta);
        if(next_pan != servo_pan) {
            servo_pan = next_pan;
            write_pan_servo();
        }
    }
    if(tilt_delta != 0) {
        const int16_t next_tilt = clamp_angle(servo_tilt + tilt_delta);
        if(next_tilt != servo_tilt) {
            servo_tilt = next_tilt;
            write_tilt_servo();
        }
    }
    Serial.printf("Servo target pan=%d tilt=%d\n", servo_pan, servo_tilt);
#else
    servo_pan = clamp_angle(servo_pan + pan_delta);
    servo_tilt = clamp_angle(servo_tilt + tilt_delta);
#endif
}

int16_t supernova_devices_servo_pan_angle(void)
{
    return servo_pan;
}

int16_t supernova_devices_servo_tilt_angle(void)
{
    return servo_tilt;
}
