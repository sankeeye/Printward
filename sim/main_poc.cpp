// PandaTouch UI simulator - proof-of-concept.
// Opens an 800x480 SDL window running LVGL, to prove the toolchain + LVGL + SDL
// pipeline works on the PC before wiring in the real UI screens.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "lvgl.h"

static uint32_t sdl_tick(void) { return SDL_GetTicks(); }

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    SDL_SetMainReady();

    lv_init();
    lv_tick_set_cb(sdl_tick);

    lv_sdl_window_create(800, 480);
    lv_sdl_mouse_create();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "PandaTouch Simulator");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "LVGL + SDL pipeline OK - 800x480");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x999999), 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 200, 60);
    lv_obj_center(btn);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2ecc71), 0);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "It works!");
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_18, 0);
    lv_obj_center(bl);

    while (1) {
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
