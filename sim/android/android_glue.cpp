// Android glue layer. Like the PC simulator's mocks.cpp, but the Bambu MQTT
// side is REAL here (the real src/bambu_mqtt.cpp is compiled in and provides
// g_printer_status + bambu_cmd_* + bambu_mqtt_setup/loop), so this file must NOT
// define those. It provides everything else the UI links against: the storage
// globals, a config-file loader, and a placeholder FTP listing.
//
// The printer IP / serial / ACCESS CODE are read at runtime from
// /sdcard/printward.conf on the tablet - the access code is a secret and never
// lives in source or git. See printward.conf.example.
#include "storage.h"
#include "bambu_ftp.h"
#include "ui_screensaver.h"   // g_screensaver_3d (persisted below)
#include <ctime>              // fallback seed in webui_pass_ensure()
#include <WiFi.h>
#include <Arduino.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// --- singletons the shims declare `extern` ---
SerialClass Serial;
WiFiClass WiFi;
EspClass ESP;

// --- storage globals (storage.h) ---
char g_wifi_ssid[32] = "";
char g_wifi_pass[64] = "";
String g_wifi_status = "Connected";
String g_ip_addr = "0.0.0.0";
uint8_t g_brightness = 70;
uint32_t g_bg_color = 0x101418;
char g_printer_ip[40] = "";
char g_printer_serial[32] = "";
char g_printer_access_code[24] = "";
char g_scale_ip[40] = "192.168.2.60";   // Printward Scale host (scale_ip= in the conf)
char g_ntfy_topic[64] = "";             // ntfy.sh topic for push notifications
char g_lang[8] = "en";                  // UI language (lang= in the conf, see lang.h)
char g_webui_pass[24] = "";             // web password; generated on first run, shown in Settings
bool g_allow_remote = false;            // refuse anything off the local network unless set
int  g_webui_port = 8080;               // web server port (webui_port= in the conf)
bool g_lan_mode = false;                // printer in LAN-only mode: expose temp + print-start
int  g_dry_interval_days = 0;           // silica-gel drying reminder interval in days (0 = off)
int  g_dry_advance_days = 0;            // show the reminder this many days BEFORE it's due
long g_dry_last_dried = 0;              // unix time the desiccant was last dried (0 = never)
bool g_dry_notified = false;            // already pushed the "dry it" reminder for this cycle
bool g_dry_banner_always = false;       // keep the dashboard banner visible at all times (vs only within the advance window)
bool g_dry_use_humidity = true;         // also remind when the AMS itself reports the desiccant is wet
bool g_dry_hum_alarm = false;           // runtime: AMS has reported "wet" for long enough (set by dry_humidity_alarm())
bool g_dry_hum_ack = false;             // "just dried" - mute the humidity alarm until the AMS reads dry again (sensor lags)
float g_tray_capacity_g[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{0}};
float g_tray_used_g[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{0}};
float g_ext_capacity_g = 0;
float g_ext_used_g = 0;

// pt_display.h declares this extern; define it so anything that touches it links.
volatile bool pt_display_suspended = false;

// --- storage functions ---
void init_storage() {}
void save_tray_weight(int, int) {}
void save_ext_weight() {}

// Writes the current settings back to /sdcard/printward.conf (the on-screen
// Printer setup form saves through here). The access code lands in this file
// on the tablet only - never in git.
void save_settings() {
    const char* path = "/sdcard/printward.conf";
    FILE* f = fopen(path, "w");
    if (!f) {
        Serial.printf("CONF: save failed - can't write %s\n", path);
        return;
    }
    fprintf(f, "printer_ip=%s\n", g_printer_ip);
    fprintf(f, "printer_serial=%s\n", g_printer_serial);
    fprintf(f, "access_code=%s\n", g_printer_access_code);
    fprintf(f, "brightness=%d\n", (int)g_brightness);
    fprintf(f, "view3d=%d\n", g_screensaver_3d ? 1 : 0);
    fprintf(f, "saver_delay=%d\n", g_screensaver_delay_s);
    fprintf(f, "scale_ip=%s\n", g_scale_ip);
    fprintf(f, "ntfy_topic=%s\n", g_ntfy_topic);
    fprintf(f, "lang=%s\n", g_lang);
    fprintf(f, "webui_pass=%s\n", g_webui_pass);
    fprintf(f, "allow_remote=%d\n", g_allow_remote ? 1 : 0);
    fprintf(f, "webui_port=%d\n", g_webui_port);
    fprintf(f, "lan_mode=%d\n", g_lan_mode ? 1 : 0);
    fprintf(f, "dry_interval=%d\n", g_dry_interval_days);
    fprintf(f, "dry_advance=%d\n", g_dry_advance_days);
    fprintf(f, "dry_last=%ld\n", g_dry_last_dried);
    fprintf(f, "dry_notified=%d\n", g_dry_notified ? 1 : 0);
    fprintf(f, "dry_banner_always=%d\n", g_dry_banner_always ? 1 : 0);
    fprintf(f, "dry_use_humidity=%d\n", g_dry_use_humidity ? 1 : 0);
    fprintf(f, "dry_hum_ack=%d\n", g_dry_hum_ack ? 1 : 0);
    if (g_wifi_ssid[0]) fprintf(f, "wifi_ssid=%s\n", g_wifi_ssid);
    fclose(f);
    Serial.println("CONF: saved /sdcard/printward.conf");
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = 0;
    return s;
}

// Reads /sdcard/printward.conf - simple key=value lines, '#' comments. The
// access code stays in this file on the tablet, never in source/git.
void load_settings() {
    const char *path = "/sdcard/printward.conf";
    FILE *f = fopen(path, "r");
    if (!f) {
        Serial.printf("CONF: %s not found - put printer_ip/printer_serial/access_code there\n", path);
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        if (*p == '#' || *p == 0) continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = trim(p);
        char *val = trim(eq + 1);
        if (!strcmp(key, "printer_ip"))          strncpy(g_printer_ip, val, sizeof(g_printer_ip) - 1);
        else if (!strcmp(key, "printer_serial")) strncpy(g_printer_serial, val, sizeof(g_printer_serial) - 1);
        else if (!strcmp(key, "access_code"))    strncpy(g_printer_access_code, val, sizeof(g_printer_access_code) - 1);
        else if (!strcmp(key, "brightness"))     g_brightness = (uint8_t)atoi(val);
        else if (!strcmp(key, "wifi_ssid"))      strncpy(g_wifi_ssid, val, sizeof(g_wifi_ssid) - 1);
        else if (!strcmp(key, "view3d"))         g_screensaver_3d = (atoi(val) != 0);
        else if (!strcmp(key, "saver_delay"))    g_screensaver_delay_s = atoi(val);
        else if (!strcmp(key, "scale_ip"))       strncpy(g_scale_ip, val, sizeof(g_scale_ip) - 1);
        else if (!strcmp(key, "ntfy_topic"))     strncpy(g_ntfy_topic, val, sizeof(g_ntfy_topic) - 1);
        else if (!strcmp(key, "lang"))           strncpy(g_lang, val, sizeof(g_lang) - 1);
        else if (!strcmp(key, "webui_pass"))     strncpy(g_webui_pass, val, sizeof(g_webui_pass) - 1);
        else if (!strcmp(key, "allow_remote"))   g_allow_remote = (atoi(val) != 0);
        else if (!strcmp(key, "dry_interval"))   g_dry_interval_days = atoi(val);
        else if (!strcmp(key, "dry_advance"))    g_dry_advance_days = atoi(val);
        else if (!strcmp(key, "dry_last"))       g_dry_last_dried = atol(val);
        else if (!strcmp(key, "dry_notified"))   g_dry_notified = (atoi(val) != 0);
        else if (!strcmp(key, "dry_banner_always")) g_dry_banner_always = (atoi(val) != 0);
        else if (!strcmp(key, "dry_use_humidity")) g_dry_use_humidity = (atoi(val) != 0);
        else if (!strcmp(key, "dry_hum_ack")) g_dry_hum_ack = (atoi(val) != 0);
        else if (!strcmp(key, "webui_port")) {
            int p = atoi(val);
            if (p >= 1024 && p <= 65535) g_webui_port = p;   // ignore junk; keep the default
        }
        else if (!strcmp(key, "lan_mode"))       g_lan_mode = (atoi(val) != 0);
    }
    fclose(f);
    // Never log the access code itself - only whether it was provided.
    Serial.printf("CONF: ip=%s serial=%s access_code=%s\n",
                  g_printer_ip, g_printer_serial,
                  g_printer_access_code[0] ? "(set)" : "(MISSING)");
}

// The web page controls a real printer, so it cannot ship without a password -
// and a default one is the same as none. Generate a random one the first time and
// show it on the tablet, the way a TV shows a pairing code: nobody has to invent a
// password, and nobody ends up running the default.
//
// No lookalike characters (0/O, 1/l/I): this gets copied off a screen by hand.
void webui_pass_ensure() {
    if (g_webui_pass[0]) return;
    static const char AB[] = "23456789abcdefghjkmnpqrstuvwxyz";
    FILE* r = fopen("/dev/urandom", "rb");
    unsigned char b[10];
    if (r) { if (fread(b, 1, sizeof(b), r) != sizeof(b)) memset(b, 0, sizeof(b)); fclose(r); }
    else {
        // /dev/urandom missing would be very odd; fall back to the clock so we
        // still get *a* password rather than an empty one, which would mean no auth.
        unsigned s = (unsigned)time(nullptr);
        for (size_t i = 0; i < sizeof(b); i++) { s = s * 1103515245u + 12345u; b[i] = (unsigned char)(s >> 16); }
        Serial.println("AUTH: no /dev/urandom, password is only clock-random");
    }
    int n = 0;
    for (size_t i = 0; i < sizeof(b) && n < 10; i++) {
        if (n == 4) g_webui_pass[n++] = '-';          // 4 + 5, easier to read out loud
        g_webui_pass[n++] = AB[b[i] % (sizeof(AB) - 1)];
    }
    g_webui_pass[n] = 0;
    save_settings();
    Serial.println("AUTH: generated a web password - see Settings on the tablet");
}

// bambu_ftp_list() is provided by the real bambu_ftp.cpp on the tablet build.
