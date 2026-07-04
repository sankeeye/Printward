#include "storage.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <WiFi.h>

char g_wifi_ssid[32] = "";
char g_wifi_pass[64] = "";
String g_wifi_status = "Disconnected";
String g_ip_addr = "0.0.0.0";

uint8_t g_brightness = 70;
uint32_t g_bg_color = 0x101418;

// Left blank on purpose - these are personal to whoever's printer this is
// (IP, serial, and LAN-only access code), so they don't belong in source
// that might get shared/published. Set them once via the web dashboard
// (or the on-device WiFi/Settings screens) after flashing; they're saved
// to flash (NVS, "bambu" namespace) and survive reflashing this same file
// unchanged - see /api/config in webserver.cpp.
char g_printer_ip[40] = "";
char g_printer_serial[32] = "";
char g_printer_access_code[24] = "";

float g_tray_capacity_g[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{0}};
float g_tray_used_g[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{0}};
float g_ext_capacity_g = 0;
float g_ext_used_g = 0;

String g_cloud_email = "";
String g_cloud_access_token = "";
String g_cloud_last_task_key = "";

static Preferences preferences;

// Preferences keys are limited to 15 chars - trayNN naming keeps each
// unit/tray pair unique and short (e.g. "cap00", "use00" .. "cap33","use33").
static String tray_key(const char* prefix, int u, int t) {
    return String(prefix) + String(u) + String(t);
}

void init_storage() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    } else {
        Serial.println("LittleFS Mounted OK");
    }
}

void load_settings() {
    preferences.begin("bambu", false);

    g_brightness = preferences.getUChar("bright", g_brightness);
    if (g_brightness < 1) g_brightness = 10;
    if (g_brightness > 100) g_brightness = 100;
    g_bg_color = preferences.getUInt("bg", g_bg_color);

    preferences.getString("wssid", g_wifi_ssid, 31);
    preferences.getString("wpass", g_wifi_pass, 63);
    preferences.getString("p_ip", g_printer_ip, 39);
    preferences.getString("p_sn", g_printer_serial, 31);
    preferences.getString("p_ac", g_printer_access_code, 23);

    g_cloud_email = preferences.getString("c_email", "");
    g_cloud_access_token = preferences.getString("c_tok", "");
    g_cloud_last_task_key = preferences.getString("c_last", "");

    for (int u = 0; u < AMS_MAX_UNITS; u++) {
        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            g_tray_capacity_g[u][t] = preferences.getFloat(tray_key("cap", u, t).c_str(), 0);
            g_tray_used_g[u][t] = preferences.getFloat(tray_key("use", u, t).c_str(), 0);
        }
    }
    g_ext_capacity_g = preferences.getFloat("ext_cap", 0);
    g_ext_used_g = preferences.getFloat("ext_use", 0);

    preferences.end();

    // One-time migration: the old Stream Deck firmware stored WiFi
    // credentials under the "deck" Preferences namespace. If this is the
    // first boot of the Bambu firmware (nothing saved under "bambu" yet),
    // pull them over so WiFi keeps working without the user re-entering
    // anything - the web dashboard needed to change settings isn't reachable
    // at all until WiFi is up, so this has to happen automatically.
    if (strlen(g_wifi_ssid) == 0) {
        Preferences legacy;
        legacy.begin("deck", true); // read-only, ok if namespace doesn't exist
        legacy.getString("wssid", g_wifi_ssid, 31);
        legacy.getString("wpass", g_wifi_pass, 63);
        legacy.end();

        if (strlen(g_wifi_ssid) > 0) {
            Serial.println("STORAGE: Migrated WiFi credentials from legacy 'deck' namespace");
            save_settings();
        }
    }

    if (strlen(g_wifi_ssid) > 0) {
        WiFi.begin(g_wifi_ssid, g_wifi_pass);
    } else {
        Serial.println("STORAGE: No WiFi credentials configured yet. Connect to this device's fallback AP or set them via USB serial.");
    }
}

void save_settings() {
    preferences.begin("bambu", false);
    preferences.putUChar("bright", g_brightness);
    preferences.putUInt("bg", g_bg_color);
    preferences.putString("wssid", g_wifi_ssid);
    preferences.putString("wpass", g_wifi_pass);
    preferences.putString("p_ip", g_printer_ip);
    preferences.putString("p_sn", g_printer_serial);
    preferences.putString("p_ac", g_printer_access_code);
    preferences.end();
}

// Kept separate from save_settings() (rather than folded into it) because
// that one gets called on every brightness-slider drag tick - bundling ~20
// more NVS keys into that hot path would mean a burst of flash writes on
// every touch-drag. These only need saving when a spool capacity is set/reset
// or the cloud login/task bookkeeping actually changes.
void save_tray_weight(int u, int t) {
    if (u < 0 || u >= AMS_MAX_UNITS || t < 0 || t >= AMS_MAX_TRAYS) return;
    preferences.begin("bambu", false);
    preferences.putFloat(tray_key("cap", u, t).c_str(), g_tray_capacity_g[u][t]);
    preferences.putFloat(tray_key("use", u, t).c_str(), g_tray_used_g[u][t]);
    preferences.end();
}

void save_ext_weight() {
    preferences.begin("bambu", false);
    preferences.putFloat("ext_cap", g_ext_capacity_g);
    preferences.putFloat("ext_use", g_ext_used_g);
    preferences.end();
}

void save_cloud_settings() {
    preferences.begin("bambu", false);
    preferences.putString("c_email", g_cloud_email);
    preferences.putString("c_tok", g_cloud_access_token);
    preferences.putString("c_last", g_cloud_last_task_key);
    preferences.end();
}
