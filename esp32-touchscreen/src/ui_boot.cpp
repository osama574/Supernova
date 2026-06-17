#include "ui_boot.h"

#include "supernova_state.h"
#include "ui_home.h"
#include "ui_theme.h"

#include "lvgl/lvgl.h"

#if defined(ARDUINO)
#include <Arduino.h>
#define UI_BOOT_LOG(message) Serial.println(message)
#else
#define UI_BOOT_LOG(message) ((void)0)
#endif

static lv_obj_t * boot_status_label = NULL;
static lv_obj_t * boot_progress_bar = NULL;
static int32_t boot_progress_value = 0;

static lv_obj_t * boot_get_active_screen(void)
{
#if LVGL_VERSION_MAJOR >= 9
    return lv_screen_active();
#else
    return lv_scr_act();
#endif
}

static lv_obj_t * boot_add_supernova_logo(lv_obj_t * parent)
{
    lv_obj_t * logo = lv_obj_create(parent);
    lv_obj_remove_style_all(logo);
    lv_obj_set_size(logo, 100, 100);
    lv_obj_set_style_radius(logo, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(logo, ui_color_panel(), 0);
    lv_obj_set_style_bg_opa(logo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(logo, 2, 0);
    lv_obj_set_style_border_color(logo, ui_color_accent_blue(), 0);

    lv_obj_t * ring = lv_obj_create(logo);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 70, 70);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0xA78BFA), 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);

    lv_obj_t * core = lv_obj_create(logo);
    lv_obj_remove_style_all(core);
    lv_obj_set_size(core, 28, 28);
    lv_obj_center(core);
    lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(core, ui_color_accent_green(), 0);
    lv_obj_set_style_bg_opa(core, LV_OPA_COVER, 0);

    static const int8_t dot_pos[][2] = {{-30, -24}, {30, -12}, {20, 30}, {-18, 34}};
    static const uint32_t dot_color[] = {0x38BDF8, 0xF472B6, 0xF59E0B, 0xA78BFA};
    for(uint8_t i = 0; i < 4; ++i) {
        lv_obj_t * dot = lv_obj_create(logo);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_align(dot, LV_ALIGN_CENTER, dot_pos[i][0], dot_pos[i][1]);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(dot_color[i]), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }

    return logo;
}

static void boot_set_progress(int32_t value)
{
    if(boot_progress_bar != NULL) {
        lv_bar_set_value(boot_progress_bar, value, LV_ANIM_OFF);
    }

    if(boot_status_label == NULL) {
        return;
    }

    if(value < 34) {
        lv_label_set_text(boot_status_label, supernova_text("Igniting core systems", "Kernsysteme starten"));
    }
    else if(value < 72) {
        lv_label_set_text(boot_status_label, supernova_text("Aligning orbit controls", "Orbitsteuerung ausrichten"));
    }
    else if(!supernova_music_library_ready()) {
        lv_label_set_text(boot_status_label, supernova_text("Loading music library", "Musikbibliothek laden"));
    }
    else {
        lv_label_set_text(boot_status_label, supernova_text("Entering Supernova", "Supernova offnet"));
    }
}

static void boot_progress_timer_cb(lv_timer_t * timer)
{
    const bool music_ready = supernova_music_library_ready();
    if(boot_progress_value < 96 || music_ready) {
        boot_progress_value += 4;
    }
    if(boot_progress_value > 100) {
        boot_progress_value = 100;
    }

    boot_set_progress(boot_progress_value);

    if(boot_progress_value < 100 || !music_ready) {
        return;
    }

    UI_BOOT_LOG("Boot complete; freeing boot screen and opening home");
    lv_timer_delete(timer);

    boot_status_label = NULL;
    boot_progress_bar = NULL;

    lv_obj_t * current_screen = boot_get_active_screen();
    if(current_screen != NULL) {
        lv_obj_clean(current_screen);
    }

    ui_show_home_screen();
}

void ui_show_boot_screen(void)
{
    UI_BOOT_LOG("Creating boot screen");
    ui_theme_init();
    boot_status_label = NULL;
    boot_progress_bar = NULL;
    boot_progress_value = 0;

    lv_obj_t * screen = lv_obj_create(NULL);
    ui_apply_screen(screen);
    lv_obj_set_style_bg_color(screen, ui_color_bg(), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_NONE, 0);

    lv_obj_t * logo = boot_add_supernova_logo(screen);
    lv_obj_align(logo, LV_ALIGN_CENTER, -120, -18);

    lv_obj_t * title = lv_label_create(screen);
    lv_label_set_text(title, "Supernova");
    lv_obj_set_style_text_color(title, ui_color_text(), 0);
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 78, -52);

    lv_obj_t * subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, supernova_text("Starting to reform the Universum",
                                               "Universum wird neu geformt"));
    lv_obj_set_style_text_color(subtitle, ui_color_muted(), 0);
    lv_obj_set_style_text_font(subtitle, ui_font_body(), 0);
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 78, -20);

    boot_status_label = lv_label_create(screen);
    lv_label_set_text(boot_status_label, supernova_text("Igniting core systems", "Kernsysteme starten"));
    lv_obj_set_style_text_color(boot_status_label, ui_color_muted(), 0);
    lv_obj_set_style_text_font(boot_status_label, ui_font_small(), 0);
    lv_obj_align(boot_status_label, LV_ALIGN_CENTER, 78, 38);

    boot_progress_bar = lv_bar_create(screen);
    lv_obj_set_size(boot_progress_bar, 218, 8);
    lv_obj_align(boot_progress_bar, LV_ALIGN_CENTER, 78, 66);
    lv_bar_set_range(boot_progress_bar, 0, 100);
    lv_bar_set_value(boot_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(boot_progress_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(boot_progress_bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(boot_progress_bar, ui_color_panel_light(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(boot_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(boot_progress_bar, ui_color_accent_green(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(boot_progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    for(uint32_t i = 0; i < 3; i++) {
        lv_obj_t * dot = lv_obj_create(screen);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 7, 7);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, ui_color_accent_green(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_opa(dot, (lv_opa_t)(LV_OPA_40 + i * 55), 0);
        lv_obj_align(dot, LV_ALIGN_CENTER, 78 + (int32_t)i * 18 - 18, 96);
    }

    ui_screen_load(screen);

    lv_timer_create(boot_progress_timer_cb, 120, NULL);
    UI_BOOT_LOG("Boot screen ready");
}
