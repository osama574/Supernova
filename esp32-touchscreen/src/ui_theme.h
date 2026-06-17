#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_theme_init(void);

typedef enum {
    UI_THEME_DARK,
    UI_THEME_LIGHT,
} ui_theme_mode_t;

void ui_theme_set_mode(ui_theme_mode_t mode);
ui_theme_mode_t ui_theme_get_mode(void);
const char * ui_theme_get_mode_name(void);

void ui_screen_load(lv_obj_t * screen);
void ui_screen_load_anim(lv_obj_t * screen);

lv_color_t ui_color_bg(void);
lv_color_t ui_color_panel(void);
lv_color_t ui_color_panel_light(void);
lv_color_t ui_color_text(void);
lv_color_t ui_color_muted(void);
lv_color_t ui_color_accent_blue(void);
lv_color_t ui_color_accent_green(void);

const lv_font_t * ui_font_small(void);
const lv_font_t * ui_font_body(void);
const lv_font_t * ui_font_subtitle(void);
const lv_font_t * ui_font_title(void);

void ui_apply_screen(lv_obj_t * screen);
void ui_apply_card(lv_obj_t * card, lv_color_t accent);
void ui_apply_header_button(lv_obj_t * button);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */
