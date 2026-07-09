// PandaTouch UI simulator entry point.
// Runs the real UI screens (from ../src) in an 800x480 SDL window, fed with the
// mock data from mocks.cpp. Mouse = touch. Lets us iterate on the UI on the PC
// without flashing the device.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstring>
#include "lvgl.h"
#include "ui_printer.h"
#include "ui_files.h"
#include "bambu_mqtt.h"

extern void sim_load_mock_data();
extern void wifi_ui_loop();   // defined in ui_wifi.cpp (not exposed in a header)

static uint32_t sdl_tick(void) { return SDL_GetTicks(); }

// Mirror PrinterApp::loop()'s action draining so the on-screen control buttons
// actually do something (log via the mock bambu_cmd_* + flip local state).
static void drain_pending_action() {
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
    SDL_SetMainReady();

    lv_init();
    lv_tick_set_cb(sdl_tick);
    lv_sdl_window_create(800, 480);
    lv_sdl_mouse_create();

    sim_load_mock_data();

    g_main_screen = lv_screen_active();
    create_printer_ui();
    update_printer_ui();

    uint32_t last_refresh = 0;
    for (;;) {
        lv_timer_handler();

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
