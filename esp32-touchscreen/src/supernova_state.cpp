#include "supernova_state.h"

#include "supernova_devices.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <time.h>
#endif

static const char * songs[] = {
    "Supernova Intro.wav",
    "Orbit Drive.mp3",
    "Nebula Pulse.mp3",
    "Mission Control.wav",
};

static const char * languages[] = {
    "English",
    "Deutsch",
};

static const char * old_default_wifi_ssid = "CHANGE_ME_OLD_SSID";
static const char * default_wifi_ssid = "CHANGE_ME_WIFI_SSID";
static const char * default_wifi_password = "CHANGE_ME_WIFI_PASSWORD";
static const char * prefs_namespace = "supernova";
static const char * prefs_language_key = "lang";
static const char * prefs_brightness_key = "bright";
static const char * prefs_wifi_ssid_key = "wifi_ssid";
static const char * prefs_wifi_password_key = "wifi_pass";
static const char * prefs_wifi_revision_key = "wifi_rev";
static const uint8_t current_wifi_revision = 2;
static const char * vienna_tz = "CET-1CEST,M3.5.0/2,M10.5.0/3";
static const char * weather_url =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude=48.2085&longitude=16.3721"
    "&current=temperature_2m,weather_code"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min"
    "&timezone=Europe%2FVienna&forecast_days=4";

static int8_t current_song = -1;
static bool music_playing = false;
static uint8_t volume_value = 55;
static uint8_t brightness_value = 85;
static supernova_led_mode_t led_mode_value = SUPERNOVA_LED_NOVA_PULSE;
static uint8_t language_index_value = 0;
static char last_wifi_ssid[33] = "";
static char pending_wifi_ssid[33] = "";
static char pending_wifi_password[65] = "";
static bool wifi_credentials_save_pending = false;
static bool time_configured = false;
static bool time_synced_value = false;
static bool screen_sleeping_value = false;
static bool screen_dimmed_value = false;
static uint32_t last_screen_activity_ms = 0;
static uint32_t last_time_sync_attempt_ms = 0;
static uint32_t last_weather_attempt_ms = 0;
static uint32_t last_weather_success_ms = 0;
static supernova_weather_t weather_value = {};
#if defined(ARDUINO)
static TaskHandle_t weather_task_handle = NULL;
#endif

static const uint32_t screen_dim_after_ms = 30000UL;
static const uint32_t screen_sleep_after_ms = 60000UL;
static const uint8_t screen_dim_brightness = 1;

static uint8_t language_count(void)
{
    return (uint8_t)(sizeof(languages) / sizeof(languages[0]));
}

static uint8_t load_language_index(void)
{
#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, true)) {
        uint8_t saved = prefs.getUChar(prefs_language_key, 0);
        prefs.end();
        if(saved < language_count()) {
            return saved;
        }
    }
#endif

    return 0;
}

static void save_language_index(uint8_t index)
{
#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, false)) {
        prefs.putUChar(prefs_language_key, index);
        prefs.end();
    }
#else
    (void)index;
#endif
}

static uint8_t load_brightness(void)
{
#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, true)) {
        uint8_t saved = prefs.getUChar(prefs_brightness_key, brightness_value);
        prefs.end();
        if(saved <= 100) {
            return saved;
        }
    }
#endif

    return brightness_value;
}

static void save_brightness(uint8_t brightness)
{
#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, false)) {
        prefs.putUChar(prefs_brightness_key, brightness);
        prefs.end();
    }
#else
    (void)brightness;
#endif
}

static void load_wifi_credentials(char * ssid, size_t ssid_size, char * password, size_t password_size)
{
    if(ssid_size > 0) {
        ssid[0] = '\0';
    }
    if(password_size > 0) {
        password[0] = '\0';
    }

#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, true)) {
        String saved_ssid = prefs.getString(prefs_wifi_ssid_key, "");
        String saved_password = prefs.getString(prefs_wifi_password_key, "");
        uint8_t saved_revision = prefs.getUChar(prefs_wifi_revision_key, 0);
        prefs.end();

        if(saved_ssid.length() > 0 && saved_ssid != old_default_wifi_ssid &&
           saved_revision == current_wifi_revision) {
            saved_ssid.toCharArray(ssid, ssid_size);
            saved_password.toCharArray(password, password_size);
            return;
        }
    }
#endif

    strncpy(ssid, default_wifi_ssid, ssid_size - 1);
    ssid[ssid_size - 1] = '\0';
    strncpy(password, default_wifi_password, password_size - 1);
    password[password_size - 1] = '\0';
}

static void save_wifi_credentials(const char * ssid, const char * password)
{
#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, false)) {
        prefs.putString(prefs_wifi_ssid_key, ssid == NULL ? "" : ssid);
        prefs.putString(prefs_wifi_password_key, password == NULL ? "" : password);
        prefs.putUChar(prefs_wifi_revision_key, current_wifi_revision);
        prefs.end();
        Serial.printf("Saved WiFi credentials for SSID: %s\n", ssid == NULL ? "" : ssid);
    }
#else
    (void)ssid;
    (void)password;
#endif
}

static int16_t celsius_x10(float value)
{
    return (int16_t)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
}

static const char * weather_summary_from_code(uint16_t code)
{
    if(code == 0) {
        return supernova_text("Clear", "Klar");
    }
    if(code <= 3) {
        return supernova_text("Cloudy", "Bewolkt");
    }
    if(code == 45 || code == 48) {
        return supernova_text("Fog", "Nebel");
    }
    if((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return supernova_text("Rain", "Regen");
    }
    if(code >= 71 && code <= 77) {
        return supernova_text("Snow", "Schnee");
    }
    if(code >= 95) {
        return supernova_text("Thunder", "Gewitter");
    }
    return supernova_text("Weather", "Wetter");
}

static void supernova_sync_time_if_needed(void)
{
#if defined(ARDUINO)
    if(!supernova_wifi_connected()) {
        return;
    }

    const uint32_t now_ms = millis();
    const uint32_t retry_ms = time_synced_value ? 3600000UL : 10000UL;
    if(last_time_sync_attempt_ms != 0 && now_ms - last_time_sync_attempt_ms < retry_ms) {
        return;
    }

    last_time_sync_attempt_ms = now_ms;
    if(!time_configured) {
        configTzTime(vienna_tz, "pool.ntp.org", "time.google.com", "time.nist.gov");
        time_configured = true;
    }

    struct tm timeinfo;
    if(getLocalTime(&timeinfo, 1200)) {
        time_synced_value = true;
        Serial.println("Vienna time synced");
    }
#endif
}

static void copy_json_string(char * target, size_t target_size, const char * source)
{
    if(target_size == 0) {
        return;
    }

    if(source == NULL) {
        target[0] = '\0';
        return;
    }

    strncpy(target, source, target_size - 1);
    target[target_size - 1] = '\0';
}

void supernova_state_init(void)
{
    language_index_value = load_language_index();
    brightness_value = load_brightness();
    screen_sleeping_value = false;
    screen_dimmed_value = false;
#if defined(ARDUINO)
    last_screen_activity_ms = millis();
#endif
    supernova_devices_init();
    supernova_devices_set_volume(volume_value);
    supernova_devices_set_brightness(brightness_value);
    supernova_devices_set_led_mode((uint8_t)led_mode_value);
    supernova_devices_music_preload_async();

    char startup_ssid[33];
    char startup_password[65];
    load_wifi_credentials(startup_ssid, sizeof(startup_ssid), startup_password, sizeof(startup_password));
    supernova_wifi_connect(startup_ssid, startup_password);
}

void supernova_restart(void)
{
#if defined(ARDUINO)
    Serial.println("Restart requested from settings");
    delay(100);
    ESP.restart();
#endif
}

void supernova_shutdown(void)
{
#if defined(ARDUINO)
    Serial.println("Screen sleep requested from settings");
#endif
    supernova_screen_sleep();
}

bool supernova_music_library_ready(void)
{
    return supernova_devices_music_preload_done();
}

void supernova_screen_sleep(void)
{
    screen_sleeping_value = true;
    screen_dimmed_value = false;
    supernova_devices_set_brightness(0);
}

void supernova_screen_wake(void)
{
    if(!screen_sleeping_value && !screen_dimmed_value) {
        return;
    }

    screen_sleeping_value = false;
    screen_dimmed_value = false;
#if defined(ARDUINO)
    last_screen_activity_ms = millis();
#endif
    supernova_devices_set_brightness(brightness_value);
#if defined(ARDUINO)
    Serial.println("Screen woke from touch");
#endif
}

bool supernova_screen_sleeping(void)
{
    return screen_sleeping_value;
}

void supernova_screen_touch_activity(void)
{
#if defined(ARDUINO)
    last_screen_activity_ms = millis();
#endif
    if(screen_dimmed_value && !screen_sleeping_value) {
        screen_dimmed_value = false;
        supernova_devices_set_brightness(brightness_value);
    }
}

static void supernova_screen_idle_poll(void)
{
#if defined(ARDUINO)
    if(screen_sleeping_value) {
        return;
    }

    const uint32_t now_ms = millis();
    if(last_screen_activity_ms == 0) {
        last_screen_activity_ms = now_ms;
        return;
    }

    const uint32_t idle_ms = now_ms - last_screen_activity_ms;
    if(idle_ms >= screen_sleep_after_ms) {
        Serial.println("Screen auto sleep after touch inactivity");
        supernova_screen_sleep();
        return;
    }

    if(idle_ms >= screen_dim_after_ms && !screen_dimmed_value) {
        screen_dimmed_value = true;
        Serial.println("Screen auto dim after touch inactivity");
        supernova_devices_set_brightness(screen_dim_brightness);
    }
#endif
}

void supernova_state_poll(void)
{
    supernova_devices_audio_poll();
    supernova_devices_led_poll();
    if(music_playing && current_song >= 0 && !supernova_devices_music_active()) {
        music_playing = false;
    }
    supernova_screen_idle_poll();
#if defined(ARDUINO)
    if(!supernova_wifi_connected() || millis() < 4500UL) {
        return;
    }

    if(wifi_credentials_save_pending) {
        String connected_ssid = WiFi.SSID();
        if(connected_ssid == pending_wifi_ssid) {
            save_wifi_credentials(pending_wifi_ssid, pending_wifi_password);
            wifi_credentials_save_pending = false;
        }
    }

    supernova_sync_time_if_needed();
#endif
}

bool supernova_wifi_connected(void)
{
#if defined(ARDUINO)
    return WiFi.status() == WL_CONNECTED;
#else
    return false;
#endif
}

bool supernova_internet_connected(void)
{
    return supernova_wifi_connected();
}

const char * supernova_wifi_ssid(void)
{
#if defined(ARDUINO)
    if(supernova_wifi_connected()) {
        static char ssid[33];
        WiFi.SSID().toCharArray(ssid, sizeof(ssid));
        return ssid;
    }
#endif
    return last_wifi_ssid[0] != '\0' ? last_wifi_ssid : "Not set";
}

const char * supernova_wifi_status_text(void)
{
#if defined(ARDUINO)
    switch(WiFi.status()) {
        case WL_CONNECTED:
            return "Connected";
        case WL_IDLE_STATUS:
            return "Idle";
        case WL_NO_SSID_AVAIL:
            return "SSID not found";
        case WL_CONNECT_FAILED:
            return "Connect failed";
        case WL_CONNECTION_LOST:
            return "Connection lost";
        case WL_DISCONNECTED:
        default:
            return "Disconnected";
    }
#else
    return "Disconnected";
#endif
}

void supernova_wifi_connect(const char * ssid, const char * password)
{
    if(ssid == NULL || ssid[0] == '\0') {
        return;
    }

    strncpy(last_wifi_ssid, ssid, sizeof(last_wifi_ssid) - 1);
    last_wifi_ssid[sizeof(last_wifi_ssid) - 1] = '\0';
    strncpy(pending_wifi_ssid, ssid, sizeof(pending_wifi_ssid) - 1);
    pending_wifi_ssid[sizeof(pending_wifi_ssid) - 1] = '\0';
    strncpy(pending_wifi_password, password == NULL ? "" : password, sizeof(pending_wifi_password) - 1);
    pending_wifi_password[sizeof(pending_wifi_password) - 1] = '\0';
    wifi_credentials_save_pending = true;

#if defined(ARDUINO)
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password == NULL ? "" : password);
    Serial.printf("WiFi connect requested: %s\n", ssid);
#endif
}

void supernova_wifi_disconnect(void)
{
#if defined(ARDUINO)
    WiFi.disconnect(false, false);
    Serial.println("WiFi disconnect requested");
#endif
}

bool supernova_wifi_get_saved_credentials(char * ssid, size_t ssid_size, char * password, size_t password_size)
{
    load_wifi_credentials(ssid, ssid_size, password, password_size);
    return ssid != NULL && ssid[0] != '\0';
}

const char * supernova_time_text(void)
{
#if defined(ARDUINO)
    static char text[6];
    if(!time_synced_value) {
        snprintf(text, sizeof(text), "00:00");
        return text;
    }

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    snprintf(text, sizeof(text), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return text;
#else
    return "00:00";
#endif
}

const char * supernova_date_text(void)
{
#if defined(ARDUINO)
    static char text[24];
    if(!time_synced_value) {
        snprintf(text, sizeof(text), "No date yet");
        return text;
    }

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(text, sizeof(text), "%a %d %b %Y", &timeinfo);
    return text;
#else
    return "No date yet";
#endif
}

const char * supernova_time_status_text(void)
{
    if(time_synced_value) {
        return supernova_wifi_connected() ? supernova_text("Internet synced", "Internet synchron")
                                          : supernova_text("Offline, time held", "Offline, Zeit lauft");
    }

    return supernova_text("Waiting for internet", "Warte auf Internet");
}

bool supernova_time_synced(void)
{
    return time_synced_value;
}

bool supernova_calendar_today(int16_t * year, uint8_t * month, uint8_t * day, uint8_t * weekday_monday0)
{
#if defined(ARDUINO)
    if(!time_synced_value) {
        return false;
    }

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    if(year != NULL) {
        *year = (int16_t)(timeinfo.tm_year + 1900);
    }
    if(month != NULL) {
        *month = (uint8_t)(timeinfo.tm_mon + 1);
    }
    if(day != NULL) {
        *day = (uint8_t)timeinfo.tm_mday;
    }
    if(weekday_monday0 != NULL) {
        *weekday_monday0 = (uint8_t)((timeinfo.tm_wday + 6) % 7);
    }
    return true;
#else
    (void)year;
    (void)month;
    (void)day;
    (void)weekday_monday0;
    return false;
#endif
}

uint8_t supernova_battery_percent(void)
{
    return 100;
}

const supernova_weather_t * supernova_weather(void)
{
    return &weather_value;
}

#if defined(ARDUINO)
static void weather_refresh_task(void * parameter)
{
    (void)parameter;
    supernova_weather_refresh();
    weather_task_handle = NULL;
    vTaskDelete(NULL);
}
#endif

void supernova_weather_refresh(void)
{
#if defined(ARDUINO)
    last_weather_attempt_ms = millis();
    if(!supernova_wifi_connected()) {
        weather_value.updating = false;
        return;
    }

    weather_value.updating = true;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(4500);
    if(!http.begin(client, weather_url)) {
        weather_value.updating = false;
        return;
    }

    const int status = http.GET();
    if(status == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if(!error) {
            float current_temp = doc["current"]["temperature_2m"] | 0.0f;
            uint16_t current_code = doc["current"]["weather_code"] | 0;
            weather_value.current_temp_c_x10 = celsius_x10(current_temp);
            weather_value.current_code = current_code;
            weather_value.today_high_c_x10 = celsius_x10(doc["daily"]["temperature_2m_max"][0] | current_temp);
            weather_value.today_low_c_x10 = celsius_x10(doc["daily"]["temperature_2m_min"][0] | current_temp);
            copy_json_string(weather_value.current_summary, sizeof(weather_value.current_summary),
                             weather_summary_from_code(current_code));
            copy_json_string(weather_value.updated_at, sizeof(weather_value.updated_at),
                             doc["current"]["time"] | "");

            for(uint8_t i = 0; i < 4; ++i) {
                copy_json_string(weather_value.daily_date[i], sizeof(weather_value.daily_date[i]),
                                 doc["daily"]["time"][i] | "");
                weather_value.daily_high_c_x10[i] = celsius_x10(doc["daily"]["temperature_2m_max"][i] | 0.0f);
                weather_value.daily_low_c_x10[i] = celsius_x10(doc["daily"]["temperature_2m_min"][i] | 0.0f);
                weather_value.daily_code[i] = doc["daily"]["weather_code"][i] | 0;
            }

            weather_value.available = true;
            last_weather_success_ms = millis();
            Serial.println("Vienna weather updated");
        }
    }

    http.end();
    weather_value.updating = false;
#endif
}

void supernova_weather_refresh_async(void)
{
#if defined(ARDUINO)
    if(!supernova_wifi_connected() || weather_value.updating || weather_task_handle != NULL) {
        return;
    }

    const uint32_t now = millis();
    if(last_weather_attempt_ms != 0 && now - last_weather_attempt_ms < 15000UL) {
        return;
    }

    weather_value.updating = true;
    if(xTaskCreatePinnedToCore(weather_refresh_task,
                               "weather",
                               8192,
                               NULL,
                               1,
                               &weather_task_handle,
                               0) != pdPASS) {
        weather_task_handle = NULL;
        weather_value.updating = false;
    }
#endif
}

uint8_t supernova_song_count(void)
{
    return supernova_devices_music_count();
}

const char * supernova_song_name(uint8_t index)
{
    if(index >= supernova_song_count()) {
        return "";
    }
    return supernova_devices_music_name(index);
}

int8_t supernova_current_song(void)
{
    return current_song;
}

bool supernova_music_playing(void)
{
    return music_playing;
}

void supernova_music_play(uint8_t index)
{
    if(index >= supernova_song_count()) {
        return;
    }

    current_song = (int8_t)index;
    music_playing = true;
#if defined(ARDUINO)
    Serial.printf("Music play: %s\n", supernova_song_name(index));
#endif
    if(!supernova_devices_play_music_preview(index)) {
        supernova_devices_start_test_tone();
    }
}

void supernova_music_toggle(void)
{
    if(current_song < 0) {
        supernova_music_play(0);
        return;
    }

    music_playing = !music_playing;
#if defined(ARDUINO)
    Serial.printf("Music %s\n", music_playing ? "playing" : "paused");
#endif
    if(music_playing) {
        if(!supernova_devices_play_music_preview((uint8_t)current_song)) {
            supernova_devices_start_test_tone();
        }
    }
    else {
        supernova_devices_stop_test_tone();
    }
}

void supernova_music_stop(void)
{
    music_playing = false;
    current_song = -1;
    supernova_devices_stop_test_tone();
#if defined(ARDUINO)
    Serial.println("Music stopped");
#endif
}

uint8_t supernova_volume(void)
{
    return volume_value;
}

void supernova_set_volume(uint8_t volume)
{
    volume_value = volume > 100 ? 100 : volume;
    supernova_devices_set_volume(volume_value);
}

uint8_t supernova_brightness(void)
{
    return brightness_value;
}

void supernova_set_brightness(uint8_t brightness)
{
    brightness_value = brightness > 100 ? 100 : brightness;
    save_brightness(brightness_value);
    if(!screen_sleeping_value && !screen_dimmed_value) {
        supernova_devices_set_brightness(brightness_value);
    }
}

supernova_led_mode_t supernova_led_mode(void)
{
    return led_mode_value;
}

const char * supernova_led_mode_name(supernova_led_mode_t mode)
{
    switch(mode) {
        case SUPERNOVA_LED_OFF:
            return "Off";
        case SUPERNOVA_LED_NOVA_PULSE:
            return "Nova pulse";
        case SUPERNOVA_LED_RAINBOW_RING:
            return "Rainbow ring";
        case SUPERNOVA_LED_ALERT:
            return "Alert";
        default:
            return "Unknown";
    }
}

void supernova_set_led_mode(supernova_led_mode_t mode)
{
    led_mode_value = mode;
    supernova_devices_set_led_mode((uint8_t)mode);
}

uint8_t supernova_language_index(void)
{
    return language_index_value;
}

const char * supernova_language_name(uint8_t index)
{
    if(index >= language_count()) {
        return "";
    }
    return languages[index];
}

void supernova_set_language_index(uint8_t index)
{
    if(index >= language_count()) {
        return;
    }

    language_index_value = index;
    save_language_index(index);
#if defined(ARDUINO)
    Serial.printf("Language set to %s\n", languages[index]);
#endif
}

const char * supernova_text(const char * english, const char * german)
{
    if(language_index_value == 1 && german != NULL && german[0] != '\0') {
        return german;
    }

    return english == NULL ? "" : english;
}
