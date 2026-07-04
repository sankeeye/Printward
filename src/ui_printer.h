#pragma once
#ifndef UI_PRINTER_H
#define UI_PRINTER_H

#include <lvgl.h>

extern lv_obj_t* g_main_screen;

// Button action ids queued by btn_event_cb() and drained in the main loop -
// keeps slow/blocking MQTT publish calls out of the LVGL redraw path (the
// same tearing fix used for the old BLE button firmware still applies here).
enum PrinterAction {
    ACT_NONE = -1,
    ACT_PAUSE_RESUME = 0,
    ACT_STOP,
    ACT_LIGHT_TOGGLE,
    ACT_SPEED_CYCLE,
    ACT_FAN_TOGGLE,
};
extern volatile int g_pending_action;

// OTA (both web-dashboard upload and ArduinoOTA/network-IDE flashing) shows a
// full-screen progress overlay; these flags let webserver.cpp/printer_app.cpp
// request it without depending on LVGL directly.
extern volatile bool g_ota_screen_requested;
extern volatile int g_ota_progress;

void create_printer_ui();
void update_printer_ui();      // call periodically from loop() to refresh live values
void update_status_label();    // wifi/IP + printer connection indicator
void show_update_screen();
void update_ota_progress(int pct, const char* msg);

#endif
