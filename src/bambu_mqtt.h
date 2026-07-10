#pragma once
#ifndef BAMBU_MQTT_H
#define BAMBU_MQTT_H

#include <Arduino.h>

#define AMS_MAX_UNITS 4
#define AMS_MAX_TRAYS 4

struct AmsTraySlot {
    bool present;
    char type[16];   // e.g. "PLA", "PETG", "ABS"
    uint32_t color;  // RGB, alpha channel from Bambu's RRGGBBAA hex dropped
    int remain;      // 0-100 estimated % remaining, -1 if unknown (third-party
                      // spools without an RFID tag never report a real value)
};

struct AmsUnit {
    bool present;
    int humidity;    // 1-5 humidity grade, -1 if unknown
    float temp;
    AmsTraySlot trays[AMS_MAX_TRAYS];
};

struct PrinterStatus {
    bool mqtt_connected = false;   // MQTT session established
    bool have_data = false;        // received at least one status push
    unsigned long last_update_ms = 0;

    char gcode_state[16] = "";     // IDLE, RUNNING, PAUSE, FINISH, FAILED, PREPARE
    char task_name[64] = "";
    char gcode_file[128] = "";     // path/name of the file being printed (if reported)
    int progress_pct = 0;
    int remaining_min = 0;
    int layer_num = 0;
    int total_layers = 0;

    float nozzle_temp = 0, nozzle_target = 0;
    float bed_temp = 0, bed_target = 0;
    float chamber_temp = 0;

    int fan_speed_pct = 0;   // part cooling fan, 0-100
    bool light_on = false;
    int speed_level = 2;     // 1=silent 2=standard 3=sport 4=ludicrous

    AmsUnit ams[AMS_MAX_UNITS];
    int ams_count = 0;

    // The single manual/external spool holder (Bambu's "vt_tray"), separate
    // from any AMS unit - used when printing without the AMS.
    AmsTraySlot external_spool;

    // Bambu's "tray_now": which tray is actively feeding the nozzle right
    // now. 255/-1 = none, 254 = external spool, otherwise (ams_id*4)+tray_id.
    // Used to guess which spool a just-finished print consumed from - see
    // bambu_cloud.h/.cpp, since the LAN status feed has no per-print weight.
    int active_tray_now = -1;
};

extern PrinterStatus g_printer_status;

// Call once from setup(), after WiFi is configured (connection itself is
// established/re-established lazily from bambu_mqtt_loop()).
void bambu_mqtt_setup();

// Call every loop() iteration. Internally throttled/non-blocking; handles
// reconnects when WiFi/printer settings are available.
void bambu_mqtt_loop();

bool bambu_is_connected();

// Call after the printer IP / serial / access code change at runtime: drops the
// current session and reconnects promptly with the new settings.
void bambu_mqtt_settings_changed();

// Records {finish time, tray that was active} whenever a print job finishes
// (gcode_state transitions away from RUNNING/PAUSE to FINISH), in a small
// ring buffer. bambu_cloud.cpp drains this to attribute cloud-reported print
// weight to the right tray - see the comment on active_tray_now above.
bool bambu_pop_pending_finish(unsigned long* finish_ms, int* tray_now);

// Commands (fire-and-forget, no acks are tracked - state changes are
// observed asynchronously via the next status push instead).
void bambu_cmd_pause();
void bambu_cmd_resume();
void bambu_cmd_stop();
void bambu_cmd_set_light(bool on);
void bambu_cmd_set_speed_level(int level);      // 1-4
void bambu_cmd_gcode(const char* gcode_line);   // e.g. "M106 P1 S255\n"

// Starts printing a .3mf project already on the printer's storage (SD card
// or its internal "cache" folder - see bambu_ftp.h for browsing those).
// `ftp_path` is the path as seen over FTP, e.g. "/my_print.3mf" or
// "/cache/my_print.3mf" - turned into the "ftp:///..." url Bambu's print
// protocol expects. `ams_mapping` must point to 5 ints using Bambu's
// right-aligned scheme (see mqtt.md's "AMS Mapping Configuration"): unused
// leading slots are -1, used slots hold this firmware's tray id
// (ams_unit*AMS_MAX_TRAYS + tray, or 254 for the external spool). Pass
// use_ams=false to print without any AMS involvement (single external
// spool, whatever was loaded at slice time).
void bambu_cmd_start_project_file(const String& ftp_path, bool use_ams, const int ams_mapping[5]);

// Starts printing a plain .gcode file already on the printer's storage
// (no AMS remapping possible this way - whatever was baked in at slice
// time is used as-is). `ftp_path` as above.
void bambu_cmd_start_gcode_file(const String& ftp_path);

#endif
