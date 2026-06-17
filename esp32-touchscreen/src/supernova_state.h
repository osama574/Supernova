#ifndef SUPERNOVA_STATE_H
#define SUPERNOVA_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SUPERNOVA_LED_OFF,
    SUPERNOVA_LED_NOVA_PULSE,
    SUPERNOVA_LED_RAINBOW_RING,
    SUPERNOVA_LED_ALERT,
} supernova_led_mode_t;

typedef struct {
    bool available;
    bool updating;
    int16_t current_temp_c_x10;
    int16_t today_high_c_x10;
    int16_t today_low_c_x10;
    uint16_t current_code;
    char current_summary[24];
    char updated_at[24];
    char daily_date[4][11];
    int16_t daily_high_c_x10[4];
    int16_t daily_low_c_x10[4];
    uint16_t daily_code[4];
} supernova_weather_t;

void supernova_state_init(void);
void supernova_state_poll(void);
void supernova_restart(void);
void supernova_shutdown(void);
bool supernova_music_library_ready(void);
void supernova_screen_sleep(void);
void supernova_screen_wake(void);
bool supernova_screen_sleeping(void);
void supernova_screen_touch_activity(void);

bool supernova_wifi_connected(void);
bool supernova_internet_connected(void);
const char * supernova_wifi_ssid(void);
const char * supernova_wifi_status_text(void);
void supernova_wifi_connect(const char * ssid, const char * password);
void supernova_wifi_disconnect(void);
bool supernova_wifi_get_saved_credentials(char * ssid, size_t ssid_size, char * password, size_t password_size);
const char * supernova_time_text(void);
const char * supernova_date_text(void);
const char * supernova_time_status_text(void);
bool supernova_time_synced(void);
bool supernova_calendar_today(int16_t * year, uint8_t * month, uint8_t * day, uint8_t * weekday_monday0);
uint8_t supernova_battery_percent(void);
const supernova_weather_t * supernova_weather(void);
void supernova_weather_refresh(void);
void supernova_weather_refresh_async(void);

uint8_t supernova_song_count(void);
const char * supernova_song_name(uint8_t index);
int8_t supernova_current_song(void);
bool supernova_music_playing(void);
void supernova_music_play(uint8_t index);
void supernova_music_toggle(void);
void supernova_music_stop(void);

uint8_t supernova_volume(void);
void supernova_set_volume(uint8_t volume);

uint8_t supernova_brightness(void);
void supernova_set_brightness(uint8_t brightness);

supernova_led_mode_t supernova_led_mode(void);
const char * supernova_led_mode_name(supernova_led_mode_t mode);
void supernova_set_led_mode(supernova_led_mode_t mode);

uint8_t supernova_language_index(void);
const char * supernova_language_name(uint8_t index);
void supernova_set_language_index(uint8_t index);
const char * supernova_text(const char * english, const char * german);

#ifdef __cplusplus
}
#endif

#endif /* SUPERNOVA_STATE_H */
