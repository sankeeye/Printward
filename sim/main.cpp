// PandaTouch UI simulator entry point.
// Runs the real UI screens (from ../src) in an 800x480 SDL window, fed with the
// mock data from mocks.cpp. Mouse = touch. Lets us iterate on the UI on the PC
// without flashing the device.
// On the PC we handle main() ourselves; on Android SDL's Java glue calls
// SDL_main (this main), so SDL_MAIN_HANDLED must NOT be defined there.
#ifndef __ANDROID__
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <cstring>
#include "lvgl.h"
#include "ui_printer.h"
#include "ui_files.h"
#include "bambu_mqtt.h"
#include "storage.h"
#include "pt/pt_display.h"
#ifdef __ANDROID__
#include "ui_tablet_setup.h"   // on-screen printer config (tablet only)
#include "ui_screensaver.h"    // idle print dashboard (tablet only)
#include "bambu_ftp.h"
#include "gcode_view.h"
#include <cstdio>
#endif

extern void sim_load_mock_data();
extern void wifi_ui_loop();   // defined in ui_wifi.cpp (not exposed in a header)

static uint32_t sdl_tick(void) { return SDL_GetTicks(); }

#ifdef __ANDROID__
// Load + parse the current print's gcode on a background thread (FTP download +
// unzip + parse would freeze the UI otherwise). Fires once per new print file.
static int gcode_load_thread(void* data) {
    gcode_view_load((const char*)data);
    free(data);
    return 0;
}
static void gcode_maybe_load() {
    static char loaded[128] = "";
    // Remember the last non-empty file so the model still shows after the print
    // finishes (the printer clears gcode_file when idle).
    static char last_seen[128] = "";
    if (!g_printer_status.mqtt_connected) return;
    const char* rep = g_printer_status.gcode_file;
    if (rep[0]) { strncpy(last_seen, rep, sizeof(last_seen) - 1); last_seen[sizeof(last_seen) - 1] = '\0'; }
    if (!last_seen[0] || strcmp(last_seen, loaded) == 0) return;   // load each new file once
    strncpy(loaded, last_seen, sizeof(loaded) - 1);
    loaded[sizeof(loaded) - 1] = '\0';
    char* copy = strdup(last_seen);
    if (copy) {
        SDL_Thread* th = SDL_CreateThread(gcode_load_thread, "gcode", copy);
        if (th) SDL_DetachThread(th); else free(copy);
    }
}
#endif

#ifdef __ANDROID__
// Software backlight for the tablet (no PWM): dim the whole UI with a
// translucent black overlay on LVGL's top layer. 100% = clear, lower = darker.
void pt_android_set_brightness(uint8_t percent) {
    static lv_obj_t* dim = nullptr;
    if (!dim) {
        dim = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(dim);
        lv_obj_set_size(dim, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(dim, lv_color_black(), 0);
        lv_obj_clear_flag(dim, LV_OBJ_FLAG_CLICKABLE);   // touches pass through
        lv_obj_clear_flag(dim, LV_OBJ_FLAG_SCROLLABLE);
    }
    if (percent > 100) percent = 100;
    int opa = (100 - (int)percent) * 235 / 100;  // never fully black (max ~92%)
    lv_obj_set_style_bg_opa(dim, (lv_opa_t)opa, 0);
    lv_obj_move_foreground(dim);
}
#endif

// Mirror PrinterApp::loop()'s action draining so the on-screen control buttons
// actually do something (log via the mock bambu_cmd_* + flip local state).
static void drain_pending_action() {
    // Speed dropdown: apply the picked level (deferred here, like the buttons).
    if (g_pending_speed_level > 0) {
        int lvl = g_pending_speed_level;
        g_pending_speed_level = 0;
        g_printer_status.speed_level = lvl;
        bambu_cmd_set_speed_level(lvl);
    }
    if (g_pending_action == ACT_NONE) return;
    int action = g_pending_action;
    g_pending_action = ACT_NONE;
    PrinterStatus &s = g_printer_status;
    switch (action) {
        case ACT_PAUSE_RESUME:
            if (strcmp(s.gcode_state, "PAUSE") == 0) { bambu_cmd_resume(); strcpy(s.gcode_state, "RUNNING"); }
            else if (strcmp(s.gcode_state, "RUNNING") == 0) { bambu_cmd_pause(); strcpy(s.gcode_state, "PAUSE"); }
            break;
        case ACT_STOP:
            bambu_cmd_stop();
            strcpy(s.gcode_state, "FINISH");
            break;
        case ACT_LIGHT_TOGGLE:
            s.light_on = !s.light_on;
            bambu_cmd_set_light(s.light_on);
            break;
        case ACT_SPEED_CYCLE: {
            int next = s.speed_level + 1; if (next > 4) next = 1;
            s.speed_level = next;
            bambu_cmd_set_speed_level(next);
            break;
        }
        default: break;
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
#ifndef __ANDROID__
    SDL_SetMainReady();
#endif

    lv_init();
    lv_tick_set_cb(sdl_tick);
    lv_display_t *disp = lv_sdl_window_create(800, 480);
    lv_sdl_mouse_create();
    (void)disp;
    SDL_DisableScreenSaver(); // keep the display on - it's a printer monitor

#ifdef __ANDROID__
    // On the tablet: read the real printer settings from /sdcard/pandatouch.conf
    // and open a real MQTT session to the printer (see android_glue.cpp).
    load_settings();
    bambu_mqtt_setup();
    create_screensaver();                    // idle dashboard (top layer, hidden)
    pt_set_backlight(g_brightness, false);   // dim overlay sits above the screensaver
#else
    sim_load_mock_data();
#endif

    g_main_screen = lv_screen_active();
    create_printer_ui();
    update_printer_ui();

#ifdef __ANDROID__
    // Not fully configured yet (no IP or no access code): land straight on the
    // setup form, with whatever is already known pre-filled.
    if (g_printer_ip[0] == '\0' || g_printer_access_code[0] == '\0') create_tablet_setup_ui();
#endif

    uint32_t last_refresh = 0;
    for (;;) {
        lv_timer_handler();

#ifdef __ANDROID__
        // Note: we intentionally let LVGL render at the tablet's native
        // resolution (the SDL driver resizes our display to the window on the
        // first surface event) rather than rendering 800x480 and upscaling -
        // upscaling blurs the text. The lv_pct/flex layouts fill the native size.
        bambu_mqtt_loop();   // real printer: reconnect + pump incoming status
        tablet_setup_loop(); // apply a queued Save from the Printer setup screen
        screensaver_loop();  // show/hide the idle print dashboard
        gcode_maybe_load();  // fetch + parse the print's gcode (background thread)
#endif
        drain_pending_action();
        ui_files_loop();
        wifi_ui_loop();

        uint32_t now = SDL_GetTicks();
        if (now - last_refresh > 500) {
            last_refresh = now;
            update_printer_ui();
        }
        SDL_Delay(5);
    }
    return 0;
}
