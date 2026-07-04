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

// Manual spool-weight tracking. capacity_g is what the user enters when
// loading a spool ("this is a fresh 1000g roll"); used_g accumulates grams
// consumed, attributed via Bambu Cloud print task history (see
// bambu_cloud.h) since the local/LAN-only status feed doesn't carry
// per-print filament usage. Remaining = capacity_g - used_g. capacity_g of
// 0 means "not set" - the Filament screen shows only Bambu's own RFID-based
// remain% (if any) until the user sets a capacity.
extern float g_tray_capacity_g[AMS_MAX_UNITS][AMS_MAX_TRAYS];
extern float g_tray_used_g[AMS_MAX_UNITS][AMS_MAX_TRAYS];
extern float g_ext_capacity_g;
extern float g_ext_used_g;

// Bambu Cloud login (optional - only needed for the manual weight tracking
// above, since that's the only place cloud data is used; LAN-only printer
// control/monitoring never needs this).
extern String g_cloud_email;
extern String g_cloud_access_token;
// Identifies the most recently-applied cloud print task, so reboots/re-polls
// don't subtract the same print's weight twice.
extern String g_cloud_last_task_key;

void init_storage();
void load_settings();
void save_settings();

// See the comment above save_tray_weight() in storage.cpp - kept separate
// from save_settings() to avoid a burst of NVS writes on every brightness
// slider tick.
void save_tray_weight(int u, int t);
void save_ext_weight();
void save_cloud_settings();

#endif
