#include "printer_app.h"
#include "storage.h"
#include "ui_printer.h"
#include "ui_wifi.h"
#include "ui_filament.h"
#include "ui_files.h"
#include "bambu_mqtt.h"
#include "bambu_cloud.h"
#include "webserver.h"
#include "pt/pt_display.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

void PrinterApp::setup() {
    WiFi.mode(WIFI_STA);

    init_storage();
    load_settings();

    g_main_screen = lv_scr_act();
    create_printer_ui();

    bambu_mqtt_setup();
    bambu_cloud_setup();

    ArduinoOTA.onStart([]() {
        pt_enter_ota_mode();
        g_ota_screen_requested = true;
        Serial.println("OTA: Start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA: Update Complete");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        (void)progress; (void)total;
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]\n", error);
    });
    ArduinoOTA.begin();
}

void PrinterApp::loop() {
    // Drain any queued button action - runs after this frame has already
    // been flushed to the display (see act_cb() in ui_printer.cpp), so the
    // MQTT publish below can't stall lv_task_handler() mid-redraw.
    if (g_pending_action != ACT_NONE) {
        int action = g_pending_action;
        g_pending_action = ACT_NONE;

        PrinterStatus& s = g_printer_status;
        switch (action) {
            case ACT_PAUSE_RESUME:
                if (strcmp(s.gcode_state, "PAUSE") == 0) bambu_cmd_resume();
                else if (strcmp(s.gcode_state, "RUNNING") == 0) bambu_cmd_pause();
                break;
            case ACT_STOP:
                bambu_cmd_stop();
                break;
            case ACT_LIGHT_TOGGLE:
                bambu_cmd_set_light(!s.light_on);
                break;
            case ACT_SPEED_CYCLE: {
                int next = s.speed_level + 1;
                if (next > 4) next = 1;
                bambu_cmd_set_speed_level(next);
                break;
            }
            case ACT_FAN_TOGGLE:
                bambu_cmd_gcode(s.fan_speed_pct > 0 ? "M106 P1 S0" : "M106 P1 S255");
                break;
        }
    }

    check_wifi_status();
    bambu_mqtt_loop();
    bambu_cloud_loop();
    wifi_ui_loop();
    ui_files_loop();

    static unsigned long last_ui_refresh = 0;
    unsigned long now = millis();
    if (now - last_ui_refresh > 500) {
        last_ui_refresh = now;
        update_printer_ui();
    }

    if (g_ota_screen_requested) {
        g_ota_screen_requested = false;
        // pt_enter_ota_mode() already suspended LVGL and drew a static
        // "Updating..." message directly via Arduino_GFX - nothing else to do.
    }

    ArduinoOTA.handle();
    yield();
}
