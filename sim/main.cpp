// FilaTrack UI simulator entry point.
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
#include "ui_filament.h"
#include "bambu_mqtt.h"
#include "storage.h"
#include "pt/pt_display.h"
#ifdef __ANDROID__
#include "ui_tablet_setup.h"   // on-screen printer config (tablet only)
#include "ui_screensaver.h"    // idle print dashboard (tablet only)
#include "ui_move.h"           // manual motion screen (live nozzle temp refresh)
#include "ui_weigh.h"          // Scale screen (live weight refresh)
#include "scale_client.h"      // background HTTP client to the FilaTrack Scale
#include "filament_track.h"    // scale-fed remaining-filament tracking
#include "spool_db.h"          // spool library + load-to-AMS
#include "notify.h"            // ntfy.sh push notifications
#include "stats.h"             // lifetime print statistics
#include "history.h"           // recent print log + cost
#include "backup.h"            // rolling on-device snapshot of all data files
#include "webctl.h"            // LAN web control server (same Move actions)
#include "bambu_ftp.h"
#include "gcode_view.h"
#include <cstdio>
#include <unistd.h>
#include <signal.h>
#endif

extern void sim_load_mock_data();
extern void wifi_ui_loop();   // defined in ui_wifi.cpp (not exposed in a header)
#ifdef __ANDROID__
extern void spools_live_loop();  // ui_spools.cpp: live-refresh the Spools list grams
#endif

static uint32_t sdl_tick(void) { return SDL_GetTicks(); }

#ifdef __ANDROID__
// Load + parse the current print's gcode on a background thread (FTP download +
// unzip + parse would freeze the UI otherwise). Retries every RETRY_MS until the
// model is actually loaded, so one slow or failed .3mf download doesn't disable
// filament tracking (and the screensaver model) for the rest of the print.
//
// The common failure is a network transition: the tablet dozes between prints,
// wakes when the next print starts, and the FTP handshake hits the still-settling
// link (mbedTLS NET_CONN_RESET / "hs -80"). So besides the periodic retry we also
// force an immediate reload the moment MQTT reconnects - that's the signal the
// link is back - instead of waiting out the retry interval.
static volatile bool g_gcode_loading = false;   // a load thread is in flight
static char g_gcode_done[128] = "";             // last file that loaded OK (set by the thread)

static int gcode_load_thread(void* data) {
    gcode_view_load((const char*)data);
    if (gcode_view_ready()) {                    // only mark done on success
        strncpy(g_gcode_done, (const char*)data, sizeof(g_gcode_done) - 1);
        g_gcode_done[sizeof(g_gcode_done) - 1] = '\0';
    }
    free(data);
    g_gcode_loading = false;
    return 0;
}
static void gcode_maybe_load() {
    static char cur[128] = "";        // the file we currently want loaded
    // Remember the last non-empty file so the model still shows after the print
    // finishes (the printer clears gcode_file when idle).
    static char last_seen[128] = "";
    static uint32_t last_try = 0;
    static bool was_connected = false;
    const uint32_t RETRY_MS = 15000;

    bool conn = g_printer_status.mqtt_connected;
    if (conn && !was_connected) last_try = 0;   // just reconnected -> retry the load now
    was_connected = conn;
    if (!conn) return;
    const char* rep = g_printer_status.gcode_file;
    if (rep[0]) { strncpy(last_seen, rep, sizeof(last_seen) - 1); last_seen[sizeof(last_seen) - 1] = '\0'; }
    if (!last_seen[0]) return;

    if (strcmp(last_seen, cur) != 0) {           // a new print file -> (re)load it
        strncpy(cur, last_seen, sizeof(cur) - 1); cur[sizeof(cur) - 1] = '\0';
        last_try = 0;                            // attempt immediately
    }
    if (strcmp(g_gcode_done, cur) == 0) return;  // already loaded this file
    if (g_gcode_loading) return;                 // a load is already running

    uint32_t now = SDL_GetTicks();
    if (last_try != 0 && (now - last_try) < RETRY_MS) return;   // wait before retrying
    last_try = now;

    char* copy = strdup(cur);
    if (copy) {
        g_gcode_loading = true;
        SDL_Thread* th = SDL_CreateThread(gcode_load_thread, "gcode", copy);
        if (th) SDL_DetachThread(th);
        else { free(copy); g_gcode_loading = false; }
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

#ifdef __ANDROID__
// Crash watchdog: fork a child before anything else. The child blocks on a pipe
// held open by us; if we CRASH, the signal handler writes a byte and the child
// relaunches the app. On a normal exit / force-stop / kill the pipe just closes
// (EOF, no byte) so the child exits without relaunching - stopping stays stopped.
static int g_wd_fd = -1;
static void crash_handler(int sig) {
    if (g_wd_fd >= 0) { char b = 1; ssize_t n = write(g_wd_fd, &b, 1); (void)n; }
    signal(sig, SIG_DFL);
    raise(sig);
}
static void install_watchdog() {
    int pfd[2];
    if (pipe(pfd) != 0) return;
    pid_t pid = fork();
    if (pid == 0) {                        // watchdog child
        close(pfd[1]);
        char b;
        ssize_t r = read(pfd[0], &b, 1);   // 1 = crash byte, 0 = EOF (normal death)
        if (r == 1) {
            execl("/system/bin/sh", "sh", "-c",
                  "sleep 3; am start -n org.libsdl.app/org.libsdl.app.SDLActivity",
                  (char*)NULL);
        }
        _exit(0);
    } else if (pid > 0) {                  // parent: keep the write end, catch crashes
        close(pfd[0]);
        g_wd_fd = pfd[1];
        signal(SIGSEGV, crash_handler);
        signal(SIGABRT, crash_handler);
        signal(SIGBUS,  crash_handler);
        signal(SIGILL,  crash_handler);
        signal(SIGFPE,  crash_handler);
    } else {
        close(pfd[0]); close(pfd[1]);
    }
}
#endif

int main(int argc, char **argv) {
    (void)argc; (void)argv;
#ifdef __ANDROID__
    install_watchdog();
#endif
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
    // On the tablet: read the real printer settings from /sdcard/filatrack.conf
    // and open a real MQTT session to the printer (see android_glue.cpp).
    migrate_legacy_data();                   // rename pre-FilaTrack files FIRST,
                                             // or an upgrade would start empty
    load_settings();
    filament_track_init();                   // load persisted spool weights
    spool_db_load();                         // load the spool library
    empty_db_load();                         // load the empty-spool weights
    stats_init();                            // load lifetime print stats
    history_init();                          // load the recent-print log
    bambu_mqtt_setup();
    webctl_start();                          // LAN control page (http://<ip>:8080)
    scale_client_start();                    // background poller for the FilaTrack Scale
    create_screensaver();                    // idle dashboard (top layer, hidden)
    filament_roll_picker_init();             // roll picker on the top layer (opened from the Dashboard AMS slots)
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
        filament_track_loop(); // tick down the active spool's remaining grams
        spools_live_loop();    // live-refresh the Spools list grams (if that tab is open)
        spool_autoclear_loop();// auto-empty a slot when its roll is pulled from the AMS
        notify_loop();       // push ntfy on print done/failed / filament short
        stats_loop();        // count finished prints + filament used
        history_loop();      // log each finished/failed print + cost
        backup_auto_loop();  // snapshot all data files to /sdcard/filatrack_backup on change
        webctl_loop();       // apply moves queued by the web control page
#endif
        drain_pending_action();
        ui_files_loop();
        wifi_ui_loop();

        uint32_t now = SDL_GetTicks();
        if (now - last_refresh > 500) {
            last_refresh = now;
            update_printer_ui();
#ifdef __ANDROID__
            update_move_ui();   // refresh live nozzle temp if the Move screen is open
            update_weigh_ui();  // refresh live weight if the Scale screen is open
#endif
        }
        SDL_Delay(5);
    }
    return 0;
}
