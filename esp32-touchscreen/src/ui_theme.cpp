#include "ui_theme.h"

#include "lvgl/lvgl.h"

#if defined(ARDUINO)
#include <Preferences.h>
#endif

static ui_theme_mode_t current_theme_mode = UI_THEME_DARK;
static bool theme_loaded = false;
static const char * prefs_namespace = "supernova";
static const char * prefs_theme_key = "theme";

static void ui_theme_save_mode(ui_theme_mode_t mode)
{
#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, false)) {
        prefs.putUChar(prefs_theme_key, (uint8_t)mode);
        prefs.end();
    }
#else
    (void)mode;
#endif
}

static lv_obj_t * ui_get_active_screen(void)
{
#if LVGL_VERSION_MAJOR >= 9
    return lv_screen_active();
#else
    return lv_scr_act();
#endif
}

static void ui_delete_screen_async(lv_obj_t * screen)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_delete_async(screen);
#else
    lv_obj_del_async(screen);
#endif
}

void ui_theme_init(void)
{
    if(theme_loaded) {
        return;
    }

    theme_loaded = true;
#if defined(ARDUINO)
    Preferences prefs;
    if(prefs.begin(prefs_namespace, true)) {
        uint8_t saved = prefs.getUChar(prefs_theme_key, (uint8_t)UI_THEME_DARK);
        prefs.end();
        if(saved <= (uint8_t)UI_THEME_LIGHT) {
            current_theme_mode = (ui_theme_mode_t)saved;
        }
    }
#endif
}

void ui_theme_set_mode(ui_theme_mode_t mode)
{
    if(mode > UI_THEME_LIGHT) {
        mode = UI_THEME_DARK;
    }

    current_theme_mode = mode;
    ui_theme_save_mode(mode);
}

ui_theme_mode_t ui_theme_get_mode(void)
{
    return current_theme_mode;
}

const char * ui_theme_get_mode_name(void)
{
    return current_theme_mode == UI_THEME_LIGHT ? "Light" : "Dark";
}

void ui_screen_load(lv_obj_t * screen)
{
    lv_obj_t * old_screen = ui_get_active_screen();

#if LVGL_VERSION_MAJOR >= 9
    lv_screen_load(screen);
#else
    lv_scr_load(screen);
#endif

    if(old_screen != NULL && old_screen != screen) {
        ui_delete_screen_async(old_screen);
    }
}

void ui_screen_load_anim(lv_obj_t * screen)
{
    ui_screen_load(screen);
}

lv_color_t ui_color_bg(void)
{
    return current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0xEFF4F8) : lv_color_hex(0x070A10);
}

lv_color_t ui_color_panel(void)
{
    return current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x121A24);
}

lv_color_t ui_color_panel_light(void)
{
    return current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0xE7EEF6) : lv_color_hex(0x1A2430);
}

lv_color_t ui_color_text(void)
{
    return current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0x111827) : lv_color_hex(0xF6F8FA);
}

lv_color_t ui_color_muted(void)
{
    return current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0x5B6573) : lv_color_hex(0x96A3B5);
}

lv_color_t ui_color_accent_blue(void)
{
    return lv_color_hex(0x39A7FF);
}

lv_color_t ui_color_accent_green(void)
{
    return lv_color_hex(0x19C37D);
}

const lv_font_t * ui_font_small(void)
{
#if LV_FONT_MONTSERRAT_14
    return &lv_font_montserrat_14;
#else
    return LV_FONT_DEFAULT;
#endif
}

const lv_font_t * ui_font_body(void)
{
#if LV_FONT_MONTSERRAT_16
    return &lv_font_montserrat_16;
#else
    return LV_FONT_DEFAULT;
#endif
}

const lv_font_t * ui_font_subtitle(void)
{
#if LV_FONT_MONTSERRAT_18
    return &lv_font_montserrat_18;
#else
    return LV_FONT_DEFAULT;
#endif
}

const lv_font_t * ui_font_title(void)
{
#if LV_FONT_MONTSERRAT_24
    return &lv_font_montserrat_24;
#elif LV_FONT_MONTSERRAT_20
    return &lv_font_montserrat_20;
#else
    return LV_FONT_DEFAULT;
#endif
}

static void ui_set_pressed_scale(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void ui_apply_screen(lv_obj_t * screen)
{
    lv_obj_remove_style_all(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, ui_color_bg(), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
}

void ui_apply_card(lv_obj_t * card, lv_color_t accent)
{
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, ui_color_panel(), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, current_theme_mode == UI_THEME_LIGHT ? 2 : 1, 0);
    lv_obj_set_style_border_color(card, accent, 0);
    lv_obj_set_style_border_opa(card, current_theme_mode == UI_THEME_LIGHT ? LV_OPA_80 : LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_shadow_spread(card, 0, 0);
    lv_obj_set_style_shadow_color(card, current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0x6B7280) : accent, 0);
    lv_obj_set_style_shadow_opa(card, current_theme_mode == UI_THEME_LIGHT ? LV_OPA_20 : LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_bg_color(card, accent, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(card, LV_OPA_30, LV_PART_MAIN | LV_STATE_PRESSED);
    ui_set_pressed_scale(card);
}

void ui_apply_header_button(lv_obj_t * button)
{
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(button, 16, 0);
    lv_obj_set_style_bg_color(button, current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x151E2A), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, current_theme_mode == UI_THEME_LIGHT ? lv_color_hex(0xD1DAE6) : lv_color_hex(0x2A3848), 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_shadow_color(button, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(button, ui_color_accent_blue(), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_PART_MAIN | LV_STATE_PRESSED);
    ui_set_pressed_scale(button);
}
