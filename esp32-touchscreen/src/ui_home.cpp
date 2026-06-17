#include "ui_home.h"

#include "supernova_camera.h"
#include "supernova_devices.h"
#include "supernova_mqtt.h"
#include "supernova_state.h"
#include "ui_theme.h"

#include "lvgl/lvgl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#define UI_HOME_LOG(message) Serial.println(message)
#else
#define UI_HOME_LOG(message) ((void)0)
#endif

typedef enum {
    UI_VIEW_HOME,
    UI_VIEW_MUSIC,
    UI_VIEW_WEATHER,
    UI_VIEW_CAMERA,
    UI_VIEW_CALENDAR,
    UI_VIEW_CLOCK,
    UI_VIEW_CALCULATOR,
    UI_VIEW_TIMER,
    UI_VIEW_SETTINGS,
    UI_VIEW_WIFI,
} ui_view_t;

typedef struct {
    const char * title_en;
    const char * title_de;
    const char * icon;
    uint32_t accent;
    ui_view_t view;
} ui_app_card_t;

typedef struct {
    const char * label;
    char action;
} calc_key_t;

static const ui_app_card_t app_cards[] = {
    { "Music", "Musik", LV_SYMBOL_AUDIO, 0x8B5CF6, UI_VIEW_MUSIC },
    { "Weather", "Wetter", LV_SYMBOL_TINT, 0x38BDF8, UI_VIEW_WEATHER },
    { "Motors", "Motoren", LV_SYMBOL_SETTINGS, 0x19C37D, UI_VIEW_CAMERA },
    { "Calendar", "Kalender", LV_SYMBOL_LIST, 0xF59E0B, UI_VIEW_CALENDAR },
    { "Clock", "Uhr", LV_SYMBOL_REFRESH, 0xF472B6, UI_VIEW_CLOCK },
    { "Calculator", "Rechner", LV_SYMBOL_KEYBOARD, 0x22D3EE, UI_VIEW_CALCULATOR },
    { "Timer", "Timer", LV_SYMBOL_LOOP, 0xFB7185, UI_VIEW_TIMER },
    { "Settings", "Setup", LV_SYMBOL_SETTINGS, 0xA3E635, UI_VIEW_SETTINGS },
};

static ui_view_t active_view = UI_VIEW_HOME;
static lv_timer_t * timer_app_timer = NULL;
static lv_timer_t * camera_app_timer = NULL;
static lv_timer_t * top_bar_timer = NULL;
static lv_timer_t * pending_nav_timer = NULL;
static ui_view_t pending_view = UI_VIEW_HOME;
static lv_obj_t * top_status_label = NULL;
static lv_obj_t * timer_label = NULL;
static lv_obj_t * camera_canvas = NULL;
static lv_obj_t * camera_status_label = NULL;
static lv_obj_t * camera_servo_label = NULL;
static uint16_t * camera_canvas_buffer = NULL;
static uint32_t camera_canvas_frame_counter = 0;
static uint32_t timer_base_seconds = 0;
static uint32_t timer_started_tick = 0;
static bool timer_running = false;
static uint8_t music_page_start = 0;
static const uint8_t music_page_rows = 3;
static const uint16_t camera_canvas_width = 260;
static const uint16_t camera_canvas_height = 150;

static lv_obj_t * calc_display_label = NULL;
static lv_obj_t * clock_time_label = NULL;
static lv_obj_t * clock_date_label = NULL;
static lv_obj_t * clock_status_label = NULL;
static double calc_left_value = 0.0;
static char calc_pending_op = 0;
static bool calc_new_entry = true;
static char calc_text[32] = "0";
static lv_obj_t * settings_keyboard = NULL;
static lv_obj_t * wifi_ssid_ta = NULL;
static lv_obj_t * wifi_password_ta = NULL;
static lv_obj_t * wifi_preview_label = NULL;
static bool weather_screen_waiting = false;

static void show_view(ui_view_t view);
static void schedule_view(ui_view_t view);
static void app_card_event_cb(lv_event_t * event);
static void back_event_cb(lv_event_t * event);
static void status_music_toggle_event_cb(lv_event_t * event);
static void status_music_close_event_cb(lv_event_t * event);
static void song_event_cb(lv_event_t * event);
static void music_rescan_event_cb(lv_event_t * event);
static void music_page_event_cb(lv_event_t * event);
static void volume_event_cb(lv_event_t * event);
static void theme_dropdown_event_cb(lv_event_t * event);
static void language_dropdown_event_cb(lv_event_t * event);
static void led_dropdown_event_cb(lv_event_t * event);
static void brightness_event_cb(lv_event_t * event);
static void wifi_textarea_event_cb(lv_event_t * event);
static void wifi_open_event_cb(lv_event_t * event);
static void wifi_connect_event_cb(lv_event_t * event);
static void wifi_disconnect_event_cb(lv_event_t * event);
static void restart_event_cb(lv_event_t * event);
static void shutdown_event_cb(lv_event_t * event);
static void keyboard_event_cb(lv_event_t * event);
static void servo_event_cb(lv_event_t * event);
static void calc_event_cb(lv_event_t * event);
static void timer_button_event_cb(lv_event_t * event);
static void deferred_nav_cb(lv_timer_t * timer);
static void top_bar_timer_cb(lv_timer_t * timer);
static void clock_tick_cb(lv_timer_t * timer);
static void timer_tick_cb(lv_timer_t * timer);
static void weather_tick_cb(lv_timer_t * timer);
static void camera_tick_cb(lv_timer_t * timer);
static void update_camera_ip_label(void);
static void update_camera_servo_label(void);
static uint32_t timer_current_seconds(void);
static void wifi_update_preview(lv_obj_t * textarea);

static lv_obj_t * make_label(lv_obj_t * parent, const char * text, const lv_font_t * font, lv_color_t color)
{
    lv_obj_t * label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static lv_obj_t * make_button(lv_obj_t * parent, int32_t w, int32_t h, const char * text, lv_color_t accent)
{
    lv_obj_t * button = lv_obj_create(parent);
    lv_obj_set_size(button, w, h);
    ui_apply_card(button, accent);

    lv_obj_t * label = make_label(button, text, ui_font_body(), ui_color_text());
    lv_obj_center(label);
    return button;
}

static void style_input(lv_obj_t * obj, lv_color_t accent)
{
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_bg_color(obj, ui_color_panel_light(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, accent, 0);
    lv_obj_set_style_text_color(obj, ui_color_text(), 0);
    lv_obj_set_style_text_font(obj, ui_font_small(), 0);
    lv_obj_set_style_pad_left(obj, 8, 0);
    lv_obj_set_style_pad_right(obj, 8, 0);
    lv_obj_set_style_pad_top(obj, 5, 0);
    lv_obj_set_style_pad_bottom(obj, 5, 0);
}

static lv_obj_t * make_dropdown(lv_obj_t * parent, const char * options, uint32_t selected, lv_color_t accent)
{
    lv_obj_t * dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options_static(dropdown, options);
    lv_dropdown_set_selected(dropdown, selected);
    lv_obj_set_size(dropdown, 132, 31);
    style_input(dropdown, accent);
    return dropdown;
}

static lv_obj_t * make_textarea(lv_obj_t * parent, const char * text, bool password)
{
    lv_obj_t * textarea = lv_textarea_create(parent);
    lv_obj_set_size(textarea, 132, 31);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_password_mode(textarea, password);
    lv_textarea_set_text(textarea, text);
    style_input(textarea, ui_color_accent_blue());
    lv_obj_add_event_cb(textarea, wifi_textarea_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(textarea, wifi_textarea_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(textarea, wifi_textarea_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    return textarea;
}

static void format_top_status(char * target, size_t target_size)
{
    snprintf(target, target_size, "%s   %s   %s",
             supernova_time_text(),
             supernova_wifi_connected() ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE,
             LV_SYMBOL_BATTERY_FULL);
}

static lv_obj_t * add_supernova_logo(lv_obj_t * parent, int32_t size)
{
    lv_obj_t * logo = lv_obj_create(parent);
    lv_obj_remove_style_all(logo);
    lv_obj_set_size(logo, size, size);
    lv_obj_set_style_radius(logo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(logo, lv_color_hex(0x08111F), 0);
    lv_obj_set_style_bg_opa(logo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(logo, 1, 0);
    lv_obj_set_style_border_color(logo, ui_color_accent_blue(), 0);

    lv_obj_t * core = lv_obj_create(logo);
    lv_obj_remove_style_all(core);
    lv_obj_set_size(core, size / 3, size / 3);
    lv_obj_center(core);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, 0);

    static const int8_t dot_pos[][2] = {{-9, -8}, {9, -4}, {-2, 10}};
    static const uint32_t dot_color[] = {0xA78BFA, 0x19C37D, 0xF59E0B};
    for(uint8_t i = 0; i < 3; ++i) {
        lv_obj_t * dot = lv_obj_create(logo);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 5, 5);
        lv_obj_align(dot, LV_ALIGN_CENTER, dot_pos[i][0], dot_pos[i][1]);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(dot_color[i]), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }

    return logo;
}

static void kill_timer_app_timer(void)
{
    supernova_camera_set_streaming(false);
    if(timer_app_timer != NULL) {
        lv_timer_delete(timer_app_timer);
        timer_app_timer = NULL;
    }
    if(camera_app_timer != NULL) {
        lv_timer_delete(camera_app_timer);
        camera_app_timer = NULL;
    }
    if(top_bar_timer != NULL) {
        lv_timer_delete(top_bar_timer);
        top_bar_timer = NULL;
    }
    top_status_label = NULL;
    timer_label = NULL;
    camera_canvas = NULL;
    camera_status_label = NULL;
    camera_servo_label = NULL;
    clock_time_label = NULL;
    clock_date_label = NULL;
    clock_status_label = NULL;
    weather_screen_waiting = false;
}

static void add_top_bar(lv_obj_t * screen, bool show_back)
{
    lv_obj_t * bar = lv_obj_create(screen);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 460, 48);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_radius(bar, 14, 0);
    lv_obj_set_style_bg_color(bar, ui_color_panel(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, ui_theme_get_mode() == UI_THEME_LIGHT ? 2 : 1, 0);
    lv_obj_set_style_border_color(bar, ui_theme_get_mode() == UI_THEME_LIGHT ? ui_color_accent_blue() : ui_color_panel_light(), 0);
    lv_obj_set_style_border_opa(bar, ui_theme_get_mode() == UI_THEME_LIGHT ? LV_OPA_60 : LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    int32_t left_x = 12;
    if(show_back) {
        lv_obj_t * back = make_button(bar, 38, 32, LV_SYMBOL_LEFT, ui_color_accent_blue());
        lv_obj_align(back, LV_ALIGN_LEFT_MID, left_x, 0);
        lv_obj_add_event_cb(back, back_event_cb, LV_EVENT_CLICKED, NULL);
        left_x += 46;
    }

    if(supernova_current_song() >= 0) {
        const char * icon = supernova_music_playing() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
        lv_obj_t * play = make_button(bar, 36, 30, icon, lv_color_hex(0x8B5CF6));
        lv_obj_align(play, LV_ALIGN_LEFT_MID, left_x, 0);
        lv_obj_add_event_cb(play, status_music_toggle_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * song = make_label(bar, supernova_song_name((uint8_t)supernova_current_song()),
                                     ui_font_small(), ui_color_text());
        lv_label_set_long_mode(song, LV_LABEL_LONG_DOT);
        lv_obj_set_width(song, 130);
        lv_obj_align(song, LV_ALIGN_LEFT_MID, left_x + 44, 0);

        lv_obj_t * close = make_button(bar, 34, 30, LV_SYMBOL_CLOSE, lv_color_hex(0xFB7185));
        lv_obj_align(close, LV_ALIGN_LEFT_MID, left_x + 182, 0);
        lv_obj_add_event_cb(close, status_music_close_event_cb, LV_EVENT_CLICKED, NULL);
    }
    else {
        lv_obj_t * logo = add_supernova_logo(bar, 34);
        lv_obj_align(logo, LV_ALIGN_LEFT_MID, left_x, 0);

        lv_obj_t * name = make_label(bar, "Supernova", ui_font_body(), ui_color_text());
        lv_obj_align(name, LV_ALIGN_LEFT_MID, left_x + 44, 0);
    }

    char right_text[64];
    format_top_status(right_text, sizeof(right_text));
    top_status_label = make_label(bar, right_text, ui_font_body(), ui_color_muted());
    lv_obj_align(top_status_label, LV_ALIGN_RIGHT_MID, -14, 0);
    top_bar_timer = lv_timer_create(top_bar_timer_cb, 1000, NULL);
}

static lv_obj_t * create_screen(bool show_back)
{
    kill_timer_app_timer();
    settings_keyboard = NULL;
    wifi_ssid_ta = NULL;
    wifi_password_ta = NULL;
    wifi_preview_label = NULL;

    lv_obj_t * screen = lv_obj_create(NULL);
    ui_apply_screen(screen);
    add_top_bar(screen, show_back);
    return screen;
}

static void add_page_title(lv_obj_t * screen, const char * title, const char * subtitle)
{
    lv_obj_t * title_label = make_label(screen, title, ui_font_title(), ui_color_text());
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 22, 68);

    lv_obj_t * subtitle_label = make_label(screen, subtitle, ui_font_small(), ui_color_muted());
    lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(subtitle_label, 420);
    lv_obj_align(subtitle_label, LV_ALIGN_TOP_LEFT, 24, 98);
}

static void add_warning(lv_obj_t * parent, const char * text, int32_t y)
{
    lv_obj_t * card = lv_obj_create(parent);
    ui_apply_card(card, lv_color_hex(0xF59E0B));
    lv_obj_set_size(card, 430, 44);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);

    lv_obj_t * label = make_label(card, LV_SYMBOL_WARNING, ui_font_body(), lv_color_hex(0xF59E0B));
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t * msg = make_label(card, text, ui_font_small(), ui_color_muted());
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 360);
    lv_obj_align(msg, LV_ALIGN_LEFT_MID, 44, 0);
}

static const char * ui_wifi_state_label(void)
{
    return supernova_wifi_connected() ? supernova_text("Connected", "Verbunden")
                                      : supernova_text("Disconnected", "Getrennt");
}

static void wifi_update_preview(lv_obj_t * textarea)
{
    if(wifi_preview_label == NULL || textarea == NULL) {
        return;
    }

    const bool password = textarea == wifi_password_ta;
    const char * field = password ? supernova_text("Password", "Passwort") : "SSID";
    const char * value = lv_textarea_get_text(textarea);
    char preview[96];
    snprintf(preview, sizeof(preview), "%s: %s", field, value == NULL ? "" : value);
    lv_label_set_text(wifi_preview_label, preview);
}

static void format_temp(char * target, size_t target_size, int16_t temp_x10)
{
    snprintf(target, target_size, "%d.%d C", temp_x10 / 10, abs(temp_x10 % 10));
}

static const char * weather_icon_for_code(uint16_t code)
{
    if((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return LV_SYMBOL_TINT;
    }
    if(code >= 95) {
        return LV_SYMBOL_WARNING;
    }
    if(code >= 71 && code <= 77) {
        return "*";
    }
    if(code >= 1 && code <= 3) {
        return LV_SYMBOL_IMAGE;
    }
    return LV_SYMBOL_OK;
}

static bool calendar_leap_year(int16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint8_t calendar_days_in_month(int16_t year, uint8_t month)
{
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(month == 2 && calendar_leap_year(year)) {
        return 29;
    }
    if(month < 1 || month > 12) {
        return 30;
    }
    return days[month - 1];
}

static const char * calendar_month_name(uint8_t month)
{
    switch(month) {
        case 1:
            return supernova_text("January", "Januar");
        case 2:
            return supernova_text("February", "Februar");
        case 3:
            return supernova_text("March", "Marz");
        case 4:
            return supernova_text("April", "April");
        case 5:
            return supernova_text("May", "Mai");
        case 6:
            return supernova_text("June", "Juni");
        case 7:
            return supernova_text("July", "Juli");
        case 8:
            return supernova_text("August", "August");
        case 9:
            return supernova_text("September", "September");
        case 10:
            return supernova_text("October", "Oktober");
        case 11:
            return supernova_text("November", "November");
        case 12:
            return supernova_text("December", "Dezember");
        default:
            return supernova_text("Calendar", "Kalender");
    }
}

static void short_date(char * target, size_t target_size, const char * iso_date, uint8_t index)
{
    if(index == 0) {
        snprintf(target, target_size, "%s", supernova_text("Today", "Heute"));
        return;
    }

    if(iso_date != NULL && strlen(iso_date) >= 10) {
        snprintf(target, target_size, "%c%c/%c%c", iso_date[5], iso_date[6], iso_date[8], iso_date[9]);
        return;
    }

    snprintf(target, target_size, "--/--");
}

static void update_clock_labels(void)
{
    if(clock_time_label != NULL) {
        lv_label_set_text(clock_time_label, supernova_time_text());
    }
    if(clock_date_label != NULL) {
        lv_label_set_text(clock_date_label, supernova_date_text());
    }
    if(clock_status_label != NULL) {
        lv_label_set_text(clock_status_label, supernova_time_status_text());
    }
}

void ui_show_home_screen(void)
{
    UI_HOME_LOG("Creating Supernova home screen");
    active_view = UI_VIEW_HOME;

    lv_obj_t * screen = create_screen(false);

    lv_obj_t * headline = make_label(screen, supernova_text("Command Center", "Kommandozentrale"),
                                     ui_font_title(), ui_color_text());
    lv_obj_align(headline, LV_ALIGN_TOP_LEFT, 20, 66);

    static int32_t col_dsc[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static int32_t row_dsc[] = { 92, 92, LV_GRID_TEMPLATE_LAST };

    lv_obj_t * grid = lv_obj_create(screen);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 448, 198);
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(grid, 10, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    for(uint8_t i = 0; i < (uint8_t)(sizeof(app_cards) / sizeof(app_cards[0])); ++i) {
        const ui_app_card_t * item = &app_cards[i];
        lv_obj_t * card = lv_obj_create(grid);
        ui_apply_card(card, lv_color_hex(item->accent));
        lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, i % 4, 1, LV_GRID_ALIGN_STRETCH, i / 4, 1);
        lv_obj_add_event_cb(card, app_card_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)item->view);

        lv_obj_set_layout(card, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(card, 2, 0);

        lv_obj_t * icon = make_label(card, item->icon, ui_font_title(), lv_color_hex(item->accent));
        LV_UNUSED(icon);
        lv_obj_t * title = make_label(card, supernova_text(item->title_en, item->title_de),
                                      ui_font_small(), ui_color_text());
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        lv_obj_set_width(title, 92);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    }

    ui_screen_load(screen);
    UI_HOME_LOG("Home screen ready");
}

static void show_music_screen(void)
{
    active_view = UI_VIEW_MUSIC;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Music", "Musik"),
                   supernova_text("SD card songs", "Songs von SD-Karte"));

    lv_obj_t * now = lv_obj_create(screen);
    lv_obj_remove_style_all(now);
    lv_obj_set_size(now, 448, 62);
    lv_obj_align(now, LV_ALIGN_TOP_MID, 0, 124);
    lv_obj_set_style_radius(now, 16, 0);
    lv_obj_set_style_bg_color(now, ui_color_panel(), 0);
    lv_obj_set_style_bg_opa(now, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(now, 1, 0);
    lv_obj_set_style_border_color(now, lv_color_hex(0x8B5CF6), 0);

    const int8_t song = supernova_current_song();
    const char * song_name = song >= 0 ? supernova_song_name((uint8_t)song)
                                       : supernova_text("No song selected", "Kein Song gewahlt");
    make_label(now, LV_SYMBOL_AUDIO, ui_font_title(), lv_color_hex(0x8B5CF6));
    lv_obj_align(lv_obj_get_child(now, 0), LV_ALIGN_LEFT_MID, 16, 0);

    lv_obj_t * song_label = make_label(now, song_name, ui_font_body(), ui_color_text());
    lv_label_set_long_mode(song_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(song_label, 210);
    lv_obj_align(song_label, LV_ALIGN_LEFT_MID, 56, -9);

    lv_obj_t * state_label = make_label(now, supernova_music_playing()
                                             ? supernova_text("Playing", "Spielt")
                                             : supernova_text("Paused", "Pausiert"),
                                        ui_font_small(), ui_color_muted());
    lv_obj_align(state_label, LV_ALIGN_LEFT_MID, 56, 14);

    lv_obj_t * toggle = make_button(now, 54, 38, supernova_music_playing() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY,
                                    lv_color_hex(0x8B5CF6));
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -128, 0);
    lv_obj_add_event_cb(toggle, status_music_toggle_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * slider = lv_slider_create(now);
    lv_obj_set_size(slider, 100, 8);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, supernova_volume(), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, volume_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    const uint8_t song_count = supernova_song_count();
    if(song_count == 0) {
        const supernova_music_scan_status_t status = supernova_devices_music_status();
        const uint8_t ignored = supernova_devices_music_ignored_count();
        const char * title_text = supernova_text("No songs found", "Keine Songs gefunden");
        char detail_text[112];

        switch(status) {
            case SUPERNOVA_MUSIC_SD_MOUNT_FAILED:
                snprintf(detail_text, sizeof(detail_text), "%s",
                         supernova_text("SD card did not mount. Check FAT32 card and slot.",
                                        "SD-Karte nicht erkannt. FAT32 und Slot prufen."));
                break;
            case SUPERNOVA_MUSIC_FOLDER_MISSING:
                snprintf(detail_text, sizeof(detail_text), "%s",
                         supernova_text("Create a /music folder on the SD card.",
                                        "Ordner /music auf der SD-Karte erstellen."));
                break;
            case SUPERNOVA_MUSIC_NO_WAV_FILES:
                if(ignored > 0) {
                    snprintf(detail_text, sizeof(detail_text), "%u %s",
                             ignored,
                             supernova_text("files ignored. Use PCM .wav files in /music.",
                                            "Dateien ignoriert. PCM .wav in /music nutzen."));
                }
                else {
                    snprintf(detail_text, sizeof(detail_text), "%s",
                             supernova_text("Put PCM .wav files directly inside /music.",
                                            "PCM .wav Dateien direkt in /music legen."));
                }
                break;
            default:
                snprintf(detail_text, sizeof(detail_text), "%s",
                         supernova_text("Press rescan after inserting the SD card.",
                                        "Nach Einlegen der SD-Karte neu scannen."));
                break;
        }

        lv_obj_t * empty = lv_obj_create(screen);
        ui_apply_card(empty, lv_color_hex(0xF59E0B));
        lv_obj_set_size(empty, 448, 92);
        lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, 198);

        lv_obj_t * icon = make_label(empty, LV_SYMBOL_WARNING, ui_font_title(), lv_color_hex(0xF59E0B));
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 16, -4);

        lv_obj_t * empty_title = make_label(empty, title_text, ui_font_body(), ui_color_text());
        lv_obj_align(empty_title, LV_ALIGN_TOP_LEFT, 58, 14);

        lv_obj_t * detail = make_label(empty, detail_text, ui_font_small(), ui_color_muted());
        lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(detail, 270);
        lv_obj_align(detail, LV_ALIGN_TOP_LEFT, 58, 42);

        lv_obj_t * rescan = make_button(empty, 94, 34, supernova_text("Rescan", "Scannen"), lv_color_hex(0xF59E0B));
        lv_obj_align(rescan, LV_ALIGN_RIGHT_MID, -14, 0);
        lv_obj_add_event_cb(rescan, music_rescan_event_cb, LV_EVENT_CLICKED, NULL);
    }

    if(song_count > 0) {
        if(music_page_start >= song_count) {
            music_page_start = 0;
        }

        lv_obj_t * list = lv_obj_create(screen);
        lv_obj_remove_style_all(list);
        lv_obj_set_size(list, 392, 116);
        lv_obj_align(list, LV_ALIGN_TOP_LEFT, 18, 196);
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(list, 0, 0);
        lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);

        const uint8_t visible_count = song_count - music_page_start < music_page_rows
                                          ? song_count - music_page_start
                                          : music_page_rows;
        for(uint8_t row_index = 0; row_index < visible_count; ++row_index) {
            const uint8_t song_index = music_page_start + row_index;
            lv_obj_t * row = lv_obj_create(list);
            ui_apply_card(row, lv_color_hex(0x8B5CF6));
            lv_obj_set_size(row, 388, 34);
            lv_obj_align(row, LV_ALIGN_TOP_MID, 0, row_index * 39);
            lv_obj_add_event_cb(row, song_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)song_index);

            lv_obj_t * title = make_label(row, supernova_song_name(song_index), ui_font_small(), ui_color_text());
            lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
            lv_obj_set_width(title, 292);
            lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

            lv_obj_t * play = make_label(row, (song == (int8_t)song_index && supernova_music_playing()) ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY,
                                         ui_font_body(), lv_color_hex(0x8B5CF6));
            lv_obj_align(play, LV_ALIGN_RIGHT_MID, -14, 0);
        }

        lv_obj_t * up = make_button(screen, 42, 48, LV_SYMBOL_UP, lv_color_hex(0x8B5CF6));
        lv_obj_align(up, LV_ALIGN_TOP_RIGHT, -18, 196);
        lv_obj_add_event_cb(up, music_page_event_cb, LV_EVENT_CLICKED, (void *)"up");
        if(music_page_start == 0) {
            lv_obj_set_style_opa(up, LV_OPA_40, 0);
        }

        lv_obj_t * down = make_button(screen, 42, 48, LV_SYMBOL_DOWN, lv_color_hex(0x8B5CF6));
        lv_obj_align(down, LV_ALIGN_TOP_RIGHT, -18, 264);
        lv_obj_add_event_cb(down, music_page_event_cb, LV_EVENT_CLICKED, (void *)"down");
        if(music_page_start + music_page_rows >= song_count) {
            lv_obj_set_style_opa(down, LV_OPA_40, 0);
        }

        char page_text[24];
        snprintf(page_text,
                 sizeof(page_text),
                 "%u-%u / %u",
                 (uint8_t)(music_page_start + 1),
                 (uint8_t)(music_page_start + visible_count),
                 song_count);
        lv_obj_t * page = make_label(screen, page_text, ui_font_small(), ui_color_muted());
        lv_obj_align(page, LV_ALIGN_BOTTOM_RIGHT, -22, -4);
    }

    ui_screen_load(screen);
}

static void show_weather_screen(void)
{
    active_view = UI_VIEW_WEATHER;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Weather", "Wetter"),
                   supernova_text("Vienna forecast", "Wetter Wien"));

    const supernova_weather_t * weather = supernova_weather();
    if(supernova_internet_connected() && !weather->available && !weather->updating) {
        supernova_weather_refresh_async();
        weather = supernova_weather();
    }

    if(!supernova_internet_connected() && !weather->available) {
        add_warning(screen, supernova_text("No internet. Weather cannot load.",
                                           "Kein Internet. Wetter fehlt."),
                    116);
    }

    lv_obj_t * card = lv_obj_create(screen);
    ui_apply_card(card, lv_color_hex(0x38BDF8));
    lv_obj_set_size(card, 448, 176);
    lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_t * title = make_label(card, "Vienna", ui_font_title(), ui_color_text());
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 18, 14);

    if(weather->available) {
        char temp[16];
        char range[32];
        format_temp(temp, sizeof(temp), weather->current_temp_c_x10);
        snprintf(range, sizeof(range), "%s %d C   %s %d C",
                 LV_SYMBOL_UP,
                 weather->today_high_c_x10 / 10,
                 LV_SYMBOL_DOWN,
                 weather->today_low_c_x10 / 10);

        lv_obj_t * icon = make_label(card, weather_icon_for_code(weather->current_code),
                                     ui_font_title(), lv_color_hex(0x38BDF8));
        lv_obj_set_style_text_font(icon, ui_font_title(), 0);
        lv_obj_align(icon, LV_ALIGN_RIGHT_MID, -44, -8);

        lv_obj_t * temp_label = make_label(card, temp, ui_font_title(), ui_color_text());
        lv_obj_align(temp_label, LV_ALIGN_TOP_LEFT, 20, 64);

        lv_obj_t * range_label = make_label(card, range, ui_font_small(), ui_color_muted());
        lv_obj_align(range_label, LV_ALIGN_TOP_LEFT, 22, 102);

        lv_obj_t * date = make_label(card, weather->daily_date[0], ui_font_body(), ui_color_muted());
        lv_obj_align(date, LV_ALIGN_BOTTOM_LEFT, 22, -18);

        lv_obj_t * status_icon = make_label(card, supernova_internet_connected() ? LV_SYMBOL_WIFI : LV_SYMBOL_SAVE,
                                            ui_font_body(), ui_color_muted());
        lv_obj_align(status_icon, LV_ALIGN_BOTTOM_RIGHT, -24, -18);
    }
    else {
        lv_obj_t * icon = make_label(card, LV_SYMBOL_WIFI, ui_font_title(), lv_color_hex(0x38BDF8));
        lv_obj_align(icon, LV_ALIGN_TOP_RIGHT, -22, 20);
        lv_obj_t * desc = make_label(card, weather->updating ?
                                     supernova_text("Weather is updating.",
                                                    "Wetter wird geladen.") :
                                     supernova_text("Connect WiFi to fetch Vienna weather.",
                                                    "WLAN verbinden fur Wetter Wien."),
                                     ui_font_body(), ui_color_muted());
        lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(desc, 330);
        lv_obj_align(desc, LV_ALIGN_BOTTOM_LEFT, 18, -20);
    }

    ui_screen_load(screen);

    weather_screen_waiting = weather->updating && !weather->available;
    if(weather_screen_waiting) {
        timer_app_timer = lv_timer_create(weather_tick_cb, 500, NULL);
    }
}

static void show_camera_screen(void)
{
    active_view = UI_VIEW_CAMERA;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Motor Control", "Motorsteuerung"),
                   supernova_text("Pan and tilt", "Links/rechts und hoch/runter"));

    supernova_camera_set_streaming(false);

    lv_obj_t * panel = lv_obj_create(screen);
    ui_apply_card(panel, lv_color_hex(0x19C37D));
    lv_obj_set_size(panel, 448, 184);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t * icon = make_label(panel, LV_SYMBOL_SETTINGS, ui_font_title(), lv_color_hex(0x19C37D));
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 26, -32);

    lv_obj_t * title = make_label(panel, supernova_text("Servo position", "Servo-Position"),
                                  ui_font_body(), ui_color_text());
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 78, 28);

    camera_servo_label = make_label(panel, "", ui_font_small(), ui_color_muted());
    lv_obj_set_width(camera_servo_label, 180);
    lv_obj_align(camera_servo_label, LV_ALIGN_TOP_LEFT, 78, 58);
    update_camera_servo_label();

    camera_status_label = make_label(panel, "", ui_font_small(), ui_color_muted());
    lv_label_set_long_mode(camera_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(camera_status_label, 190);
    lv_obj_align(camera_status_label, LV_ALIGN_TOP_LEFT, 78, 92);
    update_camera_ip_label();

    static const struct {
        const char * text;
        int8_t pan;
        int8_t tilt;
        int32_t x;
        int32_t y;
    } controls[] = {
        { LV_SYMBOL_UP, 0, -1, 92, 0 },
        { LV_SYMBOL_LEFT, -1, 0, 36, 50 },
        { LV_SYMBOL_RIGHT, 1, 0, 148, 50 },
        { LV_SYMBOL_DOWN, 0, 1, 92, 100 },
    };

    for(uint8_t i = 0; i < 4; ++i) {
        lv_obj_t * button = make_button(panel, 54, 46, controls[i].text, lv_color_hex(0x19C37D));
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 252 + controls[i].x, 18 + controls[i].y);
        lv_obj_add_event_cb(button, servo_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(button, servo_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(uintptr_t)i);
    }

    ui_screen_load(screen);
    camera_app_timer = lv_timer_create(camera_tick_cb, 1000, NULL);
}

static void show_calendar_screen(void)
{
    active_view = UI_VIEW_CALENDAR;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Calendar", "Kalender"),
                   supernova_text("Monthly view", "Monatsansicht"));

    lv_obj_t * panel = lv_obj_create(screen);
    ui_apply_card(panel, lv_color_hex(0xF59E0B));
    lv_obj_set_size(panel, 448, 176);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -8);

    int16_t year = 0;
    uint8_t month_number = 0;
    uint8_t today = 0;
    uint8_t today_weekday = 0;
    const bool date_ready = supernova_calendar_today(&year, &month_number, &today, &today_weekday);

    char month_text[32];
    if(date_ready) {
        snprintf(month_text, sizeof(month_text), "%s %d", calendar_month_name(month_number), year);
    }
    else {
        snprintf(month_text, sizeof(month_text), "%s", supernova_text("Waiting for time", "Warte auf Zeit"));
    }

    lv_obj_t * month = make_label(panel, month_text, ui_font_body(), ui_color_text());
    lv_obj_align(month, LV_ALIGN_TOP_MID, 0, 12);

    if(!date_ready) {
        lv_obj_t * hint = make_label(panel,
                                     supernova_text("Connect WiFi to sync the current day.",
                                                    "WLAN verbinden, um den Tag zu synchronisieren."),
                                     ui_font_small(),
                                     ui_color_muted());
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(hint, 330);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 26);
        ui_screen_load(screen);
        return;
    }

    const char * days[] = {"M", "T", "W", "T", "F", "S", "S"};
    for(uint8_t i = 0; i < 7; ++i) {
        lv_obj_t * d = make_label(panel, days[i], ui_font_small(), ui_color_muted());
        lv_obj_align(d, LV_ALIGN_TOP_LEFT, 34 + i * 58, 46);
    }

    const uint8_t days_in_month = calendar_days_in_month(year, month_number);
    const uint8_t first_weekday = (uint8_t)((today_weekday + 35 - ((today - 1) % 7)) % 7);
    for(uint8_t i = 1; i <= days_in_month; ++i) {
        char text[4];
        snprintf(text, sizeof(text), "%u", i);
        uint8_t index = first_weekday + i - 1;
        const int32_t x = 27 + (index % 7) * 58;
        const int32_t y = 66 + (index / 7) * 18;
        if(i == today) {
            lv_obj_t * cell = lv_obj_create(panel);
            lv_obj_remove_style_all(cell);
            lv_obj_set_size(cell, 34, 18);
            lv_obj_align(cell, LV_ALIGN_TOP_LEFT, x - 5, y - 1);
            lv_obj_set_style_radius(cell, 8, 0);
            lv_obj_set_style_bg_color(cell, lv_color_hex(0xF59E0B), 0);
            lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
            lv_obj_t * d = make_label(cell, text, ui_font_small(), lv_color_hex(0x111827));
            lv_obj_center(d);
        }
        else {
            lv_obj_t * d = make_label(panel, text, ui_font_small(), ui_color_text());
            lv_obj_align(d, LV_ALIGN_TOP_LEFT, x, y);
        }
    }

    ui_screen_load(screen);
}

static void show_clock_screen(void)
{
    active_view = UI_VIEW_CLOCK;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Clock", "Uhr"),
                   supernova_text("24-hour time", "24-Stunden-Zeit"));

    lv_obj_t * panel = lv_obj_create(screen);
    ui_apply_card(panel, lv_color_hex(0xF472B6));
    lv_obj_set_size(panel, 448, 182);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_t * orbit = lv_obj_create(panel);
    lv_obj_remove_style_all(orbit);
    lv_obj_set_size(orbit, 126, 126);
    lv_obj_align(orbit, LV_ALIGN_RIGHT_MID, -22, 2);
    lv_obj_set_style_radius(orbit, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(orbit, 2, 0);
    lv_obj_set_style_border_color(orbit, lv_color_hex(0xF472B6), 0);
    lv_obj_set_style_bg_color(orbit, ui_color_panel_light(), 0);
    lv_obj_set_style_bg_opa(orbit, LV_OPA_COVER, 0);

    lv_obj_t * inner = lv_obj_create(orbit);
    lv_obj_remove_style_all(inner);
    lv_obj_set_size(inner, 72, 72);
    lv_obj_center(inner);
    lv_obj_set_style_radius(inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(inner, 1, 0);
    lv_obj_set_style_border_color(inner, ui_color_accent_blue(), 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_TRANSP, 0);

    static const int8_t dot_pos[][2] = {{0, -48}, {43, -14}, {25, 40}, {-34, 33}, {-47, -12}};
    static const uint32_t dot_color[] = {0xF472B6, 0x38BDF8, 0x19C37D, 0xF59E0B, 0xA78BFA};
    for(uint8_t i = 0; i < 5; ++i) {
        lv_obj_t * dot = lv_obj_create(orbit);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, i == 0 ? 13 : 8, i == 0 ? 13 : 8);
        lv_obj_align(dot, LV_ALIGN_CENTER, dot_pos[i][0], dot_pos[i][1]);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(dot_color[i]), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }

    lv_obj_t * city = make_label(panel, "Vienna", ui_font_body(), ui_color_muted());
    lv_obj_align(city, LV_ALIGN_TOP_LEFT, 22, 20);

    clock_time_label = make_label(panel, supernova_time_text(), ui_font_title(), ui_color_text());
    lv_obj_set_style_text_letter_space(clock_time_label, 2, 0);
    lv_obj_align(clock_time_label, LV_ALIGN_TOP_LEFT, 22, 52);

    clock_date_label = make_label(panel, supernova_date_text(), ui_font_body(), ui_color_text());
    lv_obj_align(clock_date_label, LV_ALIGN_TOP_LEFT, 24, 94);

    clock_status_label = make_label(panel, supernova_time_status_text(), ui_font_small(), ui_color_muted());
    lv_obj_align(clock_status_label, LV_ALIGN_TOP_LEFT, 24, 124);

    timer_app_timer = lv_timer_create(clock_tick_cb, 1000, NULL);
    ui_screen_load(screen);
}

static void calc_set_text(const char * text)
{
    strncpy(calc_text, text, sizeof(calc_text) - 1);
    calc_text[sizeof(calc_text) - 1] = '\0';
    if(calc_display_label != NULL) {
        lv_label_set_text(calc_display_label, calc_text);
    }
}

static void calc_apply_operator(char op)
{
    double right = atof(calc_text);
    if(calc_pending_op == '+') {
        calc_left_value += right;
    }
    else if(calc_pending_op == '-') {
        calc_left_value -= right;
    }
    else if(calc_pending_op == '*') {
        calc_left_value *= right;
    }
    else if(calc_pending_op == '/') {
        calc_left_value = right == 0.0 ? 0.0 : calc_left_value / right;
    }
    else {
        calc_left_value = right;
    }

    calc_pending_op = op;
    snprintf(calc_text, sizeof(calc_text), "%.2f", calc_left_value);
    if(calc_display_label != NULL) {
        lv_label_set_text(calc_display_label, calc_text);
    }
    calc_new_entry = true;
}

static void show_calculator_screen(void)
{
    active_view = UI_VIEW_CALCULATOR;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Calculator", "Rechner"),
                   supernova_text("Simple arithmetic", "Einfach rechnen"));

    lv_obj_t * display = lv_obj_create(screen);
    ui_apply_card(display, lv_color_hex(0x22D3EE));
    lv_obj_set_size(display, 160, 150);
    lv_obj_align(display, LV_ALIGN_BOTTOM_LEFT, 20, -22);

    lv_obj_t * caption = make_label(display, supernova_text("Result", "Ergebnis"),
                                    ui_font_small(), ui_color_muted());
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 12, 12);

    calc_display_label = make_label(display, calc_text, ui_font_title(), ui_color_text());
    lv_label_set_long_mode(calc_display_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(calc_display_label, 130);
    lv_obj_align(calc_display_label, LV_ALIGN_RIGHT_MID, -14, 10);

    static const calc_key_t keys[] = {
        { "7", '7' }, { "8", '8' }, { "9", '9' }, { "÷", '/' },
        { "4", '4' }, { "5", '5' }, { "6", '6' }, { "×", '*' },
        { "1", '1' }, { "2", '2' }, { "3", '3' }, { "-", '-' },
        { "C", 'C' }, { "0", '0' }, { "=", '=' }, { "+", '+' },
    };

    for(uint8_t i = 0; i < 16; ++i) {
        lv_obj_t * key = make_button(screen, 62, 34, keys[i].label, lv_color_hex(0x22D3EE));
        lv_obj_align(key, LV_ALIGN_BOTTOM_LEFT, 204 + (i % 4) * 66, -22 - (3 - (i / 4)) * 38);
        lv_obj_add_event_cb(key, calc_event_cb, LV_EVENT_CLICKED, (void *)&keys[i]);
    }

    ui_screen_load(screen);
}

static void update_timer_label(void)
{
    if(timer_label == NULL) {
        return;
    }

    char text[16];
    const uint32_t seconds = timer_current_seconds();
    snprintf(text, sizeof(text), "%02lu:%02lu", seconds / 60UL, seconds % 60UL);
    lv_label_set_text(timer_label, text);
}

static void show_timer_screen(void)
{
    active_view = UI_VIEW_TIMER;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Timer", "Timer"),
                   supernova_text("Start, pause, reset", "Start, Pause, Reset"));

    timer_label = make_label(screen, "00:00", ui_font_title(), ui_color_text());
    lv_obj_set_style_text_letter_space(timer_label, 2, 0);
    lv_obj_align(timer_label, LV_ALIGN_CENTER, 0, -16);
    update_timer_label();

    lv_obj_t * start = make_button(screen, 106, 42,
                                   timer_running ? LV_SYMBOL_PAUSE " Pause" : LV_SYMBOL_PLAY " Start",
                                   lv_color_hex(0xFB7185));
    lv_obj_align(start, LV_ALIGN_BOTTOM_MID, -62, -40);
    lv_obj_add_event_cb(start, timer_button_event_cb, LV_EVENT_CLICKED, (void *)"toggle");

    lv_obj_t * reset = make_button(screen, 106, 42, LV_SYMBOL_REFRESH " Reset", lv_color_hex(0xFB7185));
    lv_obj_align(reset, LV_ALIGN_BOTTOM_MID, 62, -40);
    lv_obj_add_event_cb(reset, timer_button_event_cb, LV_EVENT_CLICKED, (void *)"reset");

    timer_app_timer = lv_timer_create(timer_tick_cb, 1000, NULL);
    ui_screen_load(screen);
}

static uint32_t timer_current_seconds(void)
{
    if(!timer_running) {
        return timer_base_seconds;
    }

    return timer_base_seconds + (lv_tick_get() - timer_started_tick) / 1000UL;
}

static void show_settings_screen(void)
{
    active_view = UI_VIEW_SETTINGS;
    lv_obj_t * screen = create_screen(true);
    add_page_title(screen, supernova_text("Settings", "Einstellungen"),
                   supernova_text("Core controls", "System"));

    lv_obj_t * restart = make_button(screen, 42, 32, LV_SYMBOL_REFRESH, lv_color_hex(0x38BDF8));
    lv_obj_align(restart, LV_ALIGN_TOP_RIGHT, -72, 72);
    lv_obj_add_event_cb(restart, restart_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * shutdown = make_button(screen, 42, 32, LV_SYMBOL_POWER, lv_color_hex(0xFB7185));
    lv_obj_align(shutdown, LV_ALIGN_TOP_RIGHT, -22, 72);
    lv_obj_add_event_cb(shutdown, shutdown_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * panel = lv_obj_create(screen);
    ui_apply_card(panel, ui_color_accent_green());
    lv_obj_set_size(panel, 448, 196);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -8);

    make_label(panel, supernova_text("Theme", "Design"), ui_font_small(), ui_color_muted());
    lv_obj_align(lv_obj_get_child(panel, lv_obj_get_child_count(panel) - 1), LV_ALIGN_TOP_LEFT, 14, 8);
    lv_obj_t * theme = make_dropdown(panel, supernova_text("Dark\nLight", "Dunkel\nHell"),
                                     ui_theme_get_mode() == UI_THEME_LIGHT ? 1 : 0, ui_color_accent_blue());
    lv_obj_align(theme, LV_ALIGN_TOP_LEFT, 14, 26);
    lv_obj_add_event_cb(theme, theme_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    make_label(panel, supernova_text("Language", "Sprache"), ui_font_small(), ui_color_muted());
    lv_obj_align(lv_obj_get_child(panel, lv_obj_get_child_count(panel) - 1), LV_ALIGN_TOP_LEFT, 158, 8);
    lv_obj_t * language = make_dropdown(panel, "English\nDeutsch", supernova_language_index(),
                                        lv_color_hex(0xA3E635));
    lv_obj_align(language, LV_ALIGN_TOP_LEFT, 158, 26);
    lv_obj_add_event_cb(language, language_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    make_label(panel, supernova_text("LED mode", "LED-Modus"), ui_font_small(), ui_color_muted());
    lv_obj_align(lv_obj_get_child(panel, lv_obj_get_child_count(panel) - 1), LV_ALIGN_TOP_LEFT, 302, 8);
    lv_obj_t * led = make_dropdown(panel, supernova_text("Off\nNova pulse\nRainbow ring\nAlert",
                                                         "Aus\nNova Puls\nRegenbogen\nAlarm"),
                                   (uint32_t)supernova_led_mode(),
                                   lv_color_hex(0xF472B6));
    lv_obj_align(led, LV_ALIGN_TOP_LEFT, 302, 26);
    lv_obj_add_event_cb(led, led_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * volume_label = make_label(panel, supernova_text("Volume", "Lautstarke"),
                                         ui_font_small(), ui_color_muted());
    lv_obj_align(volume_label, LV_ALIGN_TOP_LEFT, 14, 66);
    lv_obj_t * volume = lv_slider_create(panel);
    lv_obj_set_size(volume, 184, 8);
    lv_obj_align(volume, LV_ALIGN_TOP_LEFT, 14, 90);
    lv_slider_set_range(volume, 0, 100);
    lv_slider_set_value(volume, supernova_volume(), LV_ANIM_OFF);
    lv_obj_add_event_cb(volume, volume_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * bright_label = make_label(panel, supernova_text("Brightness", "Helligkeit"),
                                         ui_font_small(), ui_color_muted());
    lv_obj_align(bright_label, LV_ALIGN_TOP_LEFT, 236, 66);
    lv_obj_t * bright = lv_slider_create(panel);
    lv_obj_set_size(bright, 184, 8);
    lv_obj_align(bright, LV_ALIGN_TOP_LEFT, 236, 90);
    lv_slider_set_range(bright, 10, 100);
    lv_slider_set_value(bright, supernova_brightness(), LV_ANIM_OFF);
    lv_obj_add_event_cb(bright, brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * wifi_box = lv_obj_create(panel);
    lv_obj_remove_style_all(wifi_box);
    lv_obj_set_size(wifi_box, 410, 48);
    lv_obj_align(wifi_box, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_radius(wifi_box, 12, 0);
    lv_obj_set_style_bg_color(wifi_box, ui_color_panel_light(), 0);
    lv_obj_set_style_bg_opa(wifi_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wifi_box, 1, 0);
    lv_obj_set_style_border_color(wifi_box, supernova_wifi_connected() ? ui_color_accent_green() : lv_color_hex(0xF59E0B), 0);

    char wifi_status[96];
    snprintf(wifi_status, sizeof(wifi_status), "WiFi: %s  |  %s",
             ui_wifi_state_label(),
             supernova_wifi_connected() ? supernova_wifi_ssid()
                                        : supernova_text("not connected", "nicht verbunden"));
    lv_obj_t * wifi = make_label(wifi_box, wifi_status, ui_font_small(), ui_color_text());
    lv_label_set_long_mode(wifi, LV_LABEL_LONG_DOT);
    lv_obj_set_width(wifi, 270);
    lv_obj_align(wifi, LV_ALIGN_LEFT_MID, 12, 0);

    if(supernova_wifi_connected()) {
        lv_obj_t * disconnect = make_button(wifi_box, 104, 31,
                                            supernova_text("Disconnect", "Trennen"), lv_color_hex(0xFB7185));
        lv_obj_align(disconnect, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_add_event_cb(disconnect, wifi_disconnect_event_cb, LV_EVENT_CLICKED, NULL);
    }
    else {
        lv_obj_t * connect = make_button(wifi_box, 104, 31,
                                         supernova_text("Connect", "Verbinden"), ui_color_accent_green());
        lv_obj_align(connect, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_add_event_cb(connect, wifi_open_event_cb, LV_EVENT_CLICKED, NULL);
    }

    ui_screen_load(screen);
}

static void show_wifi_screen(void)
{
    active_view = UI_VIEW_WIFI;
    lv_obj_t * screen = create_screen(true);
    lv_obj_t * title = make_label(screen, supernova_text("WiFi setup", "WLAN einrichten"),
                                  ui_font_title(), ui_color_text());
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 22, 64);

    lv_obj_t * connect = make_button(screen, 116, 34,
                                     supernova_text("Connect", "Verbinden"), ui_color_accent_green());
    lv_obj_align(connect, LV_ALIGN_TOP_RIGHT, -22, 70);
    lv_obj_add_event_cb(connect, wifi_connect_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * ssid_label = make_label(screen, "SSID", ui_font_small(), ui_color_muted());
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 22, 100);
    wifi_ssid_ta = make_textarea(screen, "CHANGE_ME_WIFI_SSID", false);
    lv_obj_set_size(wifi_ssid_ta, 204, 30);
    lv_obj_align(wifi_ssid_ta, LV_ALIGN_TOP_LEFT, 22, 116);
    lv_textarea_set_max_length(wifi_ssid_ta, 32);

    lv_obj_t * password_label = make_label(screen, supernova_text("Password", "Passwort"),
                                           ui_font_small(), ui_color_muted());
    lv_obj_align(password_label, LV_ALIGN_TOP_LEFT, 254, 100);
    wifi_password_ta = make_textarea(screen, "CHANGE_ME_WIFI_PASSWORD", true);
    lv_obj_set_size(wifi_password_ta, 204, 30);
    lv_obj_align(wifi_password_ta, LV_ALIGN_TOP_LEFT, 254, 116);
    lv_textarea_set_max_length(wifi_password_ta, 64);

    settings_keyboard = lv_keyboard_create(screen);
    lv_obj_set_size(settings_keyboard, 470, 160);
    lv_obj_align(settings_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(settings_keyboard, ui_font_body(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(settings_keyboard, ui_color_panel(), 0);
    lv_obj_set_style_bg_opa(settings_keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_row(settings_keyboard, 4, 0);
    lv_obj_set_style_pad_column(settings_keyboard, 4, 0);
    lv_obj_add_event_cb(settings_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(settings_keyboard, keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(settings_keyboard, LV_OBJ_FLAG_HIDDEN);

    ui_screen_load(screen);
}

static void show_view(ui_view_t view)
{
    switch(view) {
        case UI_VIEW_MUSIC:
            show_music_screen();
            break;
        case UI_VIEW_WEATHER:
            show_weather_screen();
            break;
        case UI_VIEW_CAMERA:
            show_camera_screen();
            break;
        case UI_VIEW_CALENDAR:
            show_calendar_screen();
            break;
        case UI_VIEW_CLOCK:
            show_clock_screen();
            break;
        case UI_VIEW_CALCULATOR:
            show_calculator_screen();
            break;
        case UI_VIEW_TIMER:
            show_timer_screen();
            break;
        case UI_VIEW_SETTINGS:
            show_settings_screen();
            break;
        case UI_VIEW_WIFI:
            show_wifi_screen();
            break;
        case UI_VIEW_HOME:
        default:
            ui_show_home_screen();
            break;
    }
}

static void deferred_nav_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    const ui_view_t view = pending_view;
    pending_nav_timer = NULL;
    show_view(view);
}

static void schedule_view(ui_view_t view)
{
    pending_view = view;
    if(pending_nav_timer != NULL) {
        lv_timer_delete(pending_nav_timer);
    }
    pending_nav_timer = lv_timer_create(deferred_nav_cb, 25, NULL);
    lv_timer_set_repeat_count(pending_nav_timer, 1);
}

static void app_card_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        schedule_view((ui_view_t)(uintptr_t)lv_event_get_user_data(event));
    }
}

static void back_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        schedule_view(UI_VIEW_HOME);
    }
}

static void status_music_toggle_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        supernova_music_toggle();
        schedule_view(active_view);
    }
}

static void status_music_close_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        supernova_music_stop();
        schedule_view(active_view);
    }
}

static void song_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        supernova_music_play((uint8_t)(uintptr_t)lv_event_get_user_data(event));
        schedule_view(UI_VIEW_MUSIC);
    }
}

static void music_rescan_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        music_page_start = 0;
        supernova_devices_music_rescan();
        schedule_view(UI_VIEW_MUSIC);
    }
}

static void music_page_event_cb(lv_event_t * event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if(code != LV_EVENT_CLICKED && code != LV_EVENT_LONG_PRESSED_REPEAT) {
        return;
    }

    const uint8_t song_count = supernova_song_count();
    if(song_count == 0) {
        return;
    }

    const char * action = (const char *)lv_event_get_user_data(event);
    if(action != NULL && strcmp(action, "up") == 0) {
        music_page_start = music_page_start > music_page_rows ? music_page_start - music_page_rows : 0;
    }
    else if(music_page_start + music_page_rows < song_count) {
        music_page_start += music_page_rows;
        if(music_page_start >= song_count) {
            music_page_start = song_count - 1;
        }
    }

    schedule_view(UI_VIEW_MUSIC);
}

static void volume_event_cb(lv_event_t * event)
{
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(event);
    supernova_set_volume((uint8_t)lv_slider_get_value(slider));
}

static void theme_dropdown_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = (lv_obj_t *)lv_event_get_target(event);
        ui_theme_set_mode(lv_dropdown_get_selected(dropdown) == 1 ? UI_THEME_LIGHT : UI_THEME_DARK);
        schedule_view(UI_VIEW_SETTINGS);
    }
}

static void language_dropdown_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = (lv_obj_t *)lv_event_get_target(event);
        supernova_set_language_index((uint8_t)lv_dropdown_get_selected(dropdown));
        schedule_view(UI_VIEW_SETTINGS);
    }
}

static void led_dropdown_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = (lv_obj_t *)lv_event_get_target(event);
        supernova_set_led_mode((supernova_led_mode_t)lv_dropdown_get_selected(dropdown));
        schedule_view(UI_VIEW_SETTINGS);
    }
}

static void brightness_event_cb(lv_event_t * event)
{
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(event);
    supernova_set_brightness((uint8_t)lv_slider_get_value(slider));
}

static void wifi_open_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        schedule_view(UI_VIEW_WIFI);
    }
}

static void wifi_textarea_event_cb(lv_event_t * event)
{
    if(settings_keyboard == NULL) {
        return;
    }

    lv_obj_t * textarea = (lv_obj_t *)lv_event_get_target(event);
    lv_keyboard_set_textarea(settings_keyboard, textarea);
    lv_obj_clear_flag(settings_keyboard, LV_OBJ_FLAG_HIDDEN);
    wifi_update_preview(textarea);
}

static void wifi_connect_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED || wifi_ssid_ta == NULL || wifi_password_ta == NULL) {
        return;
    }

    supernova_wifi_connect(lv_textarea_get_text(wifi_ssid_ta), lv_textarea_get_text(wifi_password_ta));
    if(settings_keyboard != NULL) {
        lv_obj_add_flag(settings_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    schedule_view(UI_VIEW_SETTINGS);
}

static void wifi_disconnect_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        supernova_wifi_disconnect();
        schedule_view(UI_VIEW_SETTINGS);
    }
}

static void restart_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        supernova_restart();
    }
}

static void shutdown_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) == LV_EVENT_CLICKED) {
        supernova_shutdown();
    }
}

static void keyboard_event_cb(lv_event_t * event)
{
    LV_UNUSED(event);
    if(settings_keyboard != NULL) {
        lv_obj_add_flag(settings_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(settings_keyboard, NULL);
    }
}

static void servo_event_cb(lv_event_t * event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if(code != LV_EVENT_CLICKED && code != LV_EVENT_LONG_PRESSED_REPEAT) {
        return;
    }

    static const int8_t pan[] = {0, -2, 2, 0};
    static const int8_t tilt[] = {-2, 0, 0, 2};
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    if(index < 4) {
        supernova_devices_servo_nudge(pan[index], tilt[index]);
        update_camera_servo_label();
    }
}

static void update_camera_servo_label(void)
{
    if(camera_servo_label == NULL || !lv_obj_is_valid(camera_servo_label)) {
        return;
    }

    char text[48];
    snprintf(text,
             sizeof(text),
             "Pan %d  Tilt %d",
             supernova_devices_servo_pan_angle(),
             supernova_devices_servo_tilt_angle());
    lv_label_set_text(camera_servo_label, text);
}

static void update_camera_ip_label(void)
{
    if(camera_status_label == NULL || !lv_obj_is_valid(camera_status_label)) {
        return;
    }

    const char * ip = supernova_camera_ip();
    const char * cam_text = (ip != NULL && ip[0] != '\0') ? ip : "-";
    char text[128];
    snprintf(text,
             sizeof(text),
             "CAM IP: %s\nBroker: %s\n%s",
             cam_text,
             supernova_mqtt_broker(),
             supernova_mqtt_status_text());
    lv_label_set_text(camera_status_label, text);
}

static void calc_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const calc_key_t * key = (const calc_key_t *)lv_event_get_user_data(event);
    if(key == NULL) {
        return;
    }
    char c = key->action;

    if(c >= '0' && c <= '9') {
        if(calc_new_entry || strcmp(calc_text, "0") == 0) {
            calc_text[0] = c;
            calc_text[1] = '\0';
            calc_new_entry = false;
        }
        else if(strlen(calc_text) < sizeof(calc_text) - 1) {
            size_t len = strlen(calc_text);
            calc_text[len] = c;
            calc_text[len + 1] = '\0';
        }
        calc_set_text(calc_text);
    }
    else if(c == 'C') {
        calc_left_value = 0.0;
        calc_pending_op = 0;
        calc_new_entry = true;
        calc_set_text("0");
    }
    else if(c == '=') {
        calc_apply_operator(0);
        calc_new_entry = true;
    }
    else {
        calc_apply_operator(c);
    }
}

static void timer_button_event_cb(lv_event_t * event)
{
    if(lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const char * action = (const char *)lv_event_get_user_data(event);
    if(strcmp(action, "toggle") == 0) {
        if(timer_running) {
            timer_base_seconds = timer_current_seconds();
            timer_running = false;
        }
        else {
            timer_started_tick = lv_tick_get();
            timer_running = true;
        }
    }
    else {
        timer_running = false;
        timer_base_seconds = 0;
        timer_started_tick = lv_tick_get();
    }
    schedule_view(UI_VIEW_TIMER);
}

static void top_bar_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(top_status_label == NULL) {
        return;
    }

    char text[64];
    format_top_status(text, sizeof(text));
    lv_label_set_text(top_status_label, text);
}

static void clock_tick_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    supernova_state_poll();
    update_clock_labels();
}

static void timer_tick_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(timer_running) {
        update_timer_label();
    }
}

static void weather_tick_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if(active_view != UI_VIEW_WEATHER || !weather_screen_waiting) {
        return;
    }

    const supernova_weather_t * weather = supernova_weather();
    if(weather->available || !weather->updating) {
        weather_screen_waiting = false;
        schedule_view(UI_VIEW_WEATHER);
    }
}

static void camera_tick_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    update_camera_ip_label();

    if(camera_canvas == NULL) {
        return;
    }

    uint32_t frame_counter = camera_canvas_frame_counter;
    if(supernova_camera_preview_changed(&frame_counter)) {
        camera_canvas_frame_counter = frame_counter;
        lv_obj_invalidate(camera_canvas);
    }
}
