#pragma once
#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include "constants.h"
#include "bambu_mqtt.h"

// WiFi
extern char g_wifi_ssid[32];
extern char g_wifi_pass[64];
extern String g_wifi_status;
extern String g_ip_addr;

// Display
extern uint8_t g_brightness;
extern uint32_t g_bg_color;

// Bambu Lab printer connection (LAN-only mode)
extern char g_printer_ip[40];
extern char g_printer_serial[32];
extern char g_printer_access_code[24];
extern char g_webui_pass[24];    // web UI password (see webui_pass_ensure())
extern bool g_allow_remote;      // serve clients outside the local network too
extern int  g_webui_port;        // web server port (default 8080)
extern bool g_lan_mode;          // printer LAN-only: expose temperature + print-start
extern int  g_dry_interval_days; // silica-gel drying reminder interval, days (0 = off)
extern long g_dry_last_dried;    // unix time the desiccant was last dried
extern bool g_dry_notified;      // already reminded for the current cycle
void webui_pass_ensure();        // make one on first run; no-op afterwards

// Manual spool-weight tracking. capacity_g is what the user enters when
// loading a spool ("this is a fresh 1000g roll") or weighs with the scale;
// used_g accumulates grams consumed - estimated live from the sliced file x
// progress (see filament_track), and optionally corrected to the real figure
// per print by the Bambu Cloud weight relay (tools/bambu_weight_relay.py),
// since the local/LAN-only status feed doesn't carry per-print filament usage.
// Remaining = capacity_g - used_g. capacity_g of 0 means "not set" - the
// Filament screen shows only Bambu's own RFID-based remain% (if any) until then.
extern float g_tray_capacity_g[AMS_MAX_UNITS][AMS_MAX_TRAYS];
extern float g_tray_used_g[AMS_MAX_UNITS][AMS_MAX_TRAYS];
extern float g_ext_capacity_g;
extern float g_ext_used_g;

void init_storage();
void load_settings();
void save_settings();

// See the comment above save_tray_weight() in storage.cpp - kept separate
// from save_settings() to avoid a burst of NVS writes on every brightness
// slider tick.
void save_tray_weight(int u, int t);
void save_ext_weight();

#endif
