#ifndef SUPERNOVA_DEVICES_H
#define SUPERNOVA_DEVICES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUPERNOVA_I2S_BCLK_PIN 25
#define SUPERNOVA_I2S_LRCLK_PIN 32
#define SUPERNOVA_I2S_DOUT_PIN 26

#define SUPERNOVA_LED_DATA_PIN 21

#define SUPERNOVA_SERVO_PAN_PIN 25
#define SUPERNOVA_SERVO_TILT_PIN 32

typedef enum {
    SUPERNOVA_MUSIC_NOT_SCANNED,
    SUPERNOVA_MUSIC_SD_MOUNT_FAILED,
    SUPERNOVA_MUSIC_FOLDER_MISSING,
    SUPERNOVA_MUSIC_NO_WAV_FILES,
    SUPERNOVA_MUSIC_READY,
} supernova_music_scan_status_t;

void supernova_devices_init(void);
void supernova_devices_set_volume(uint8_t volume);
void supernova_devices_set_brightness(uint8_t brightness);
void supernova_devices_play_test_tone(void);
void supernova_devices_start_test_tone(void);
void supernova_devices_stop_test_tone(void);
void supernova_devices_audio_poll(void);
void supernova_devices_led_poll(void);
void supernova_devices_music_preload_async(void);
bool supernova_devices_music_preload_done(void);
uint8_t supernova_devices_music_count(void);
const char * supernova_devices_music_name(uint8_t index);
supernova_music_scan_status_t supernova_devices_music_status(void);
uint8_t supernova_devices_music_ignored_count(void);
void supernova_devices_music_rescan(void);
bool supernova_devices_music_active(void);
bool supernova_devices_play_music_preview(uint8_t index);
void supernova_devices_set_led_mode(uint8_t mode);
void supernova_devices_servo_nudge(int8_t pan_delta, int8_t tilt_delta);
int16_t supernova_devices_servo_pan_angle(void);
int16_t supernova_devices_servo_tilt_angle(void);

#ifdef __cplusplus
}
#endif

#endif /* SUPERNOVA_DEVICES_H */
