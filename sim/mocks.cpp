// Simulator mocks: definitions for everything the UI sources reference that
// normally lives in the hardware/network modules (storage, MQTT, FTP, WiFi).
// Fills the printer status + settings with realistic fake data so the screens
// look populated, and turns commands into console prints.
#include "storage.h"
#include "bambu_ftp.h"
#include <WiFi.h>
#include <Arduino.h>
#include <cstring>

// --- singletons the shims declare `extern` ---
SerialClass Serial;
WiFiClass WiFi;

// --- printer status (bambu_mqtt.h) ---
PrinterStatus g_printer_status;

// --- storage globals (storage.h) ---
char g_wifi_ssid[32] = "";
char g_wifi_pass[64] = "";
String g_wifi_status = "Disconnected";
String g_ip_addr = "0.0.0.0";
uint8_t g_brightness = 70;
uint32_t g_bg_color = 0x101418;
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

// pt_display.h declares this extern; define it so anything that touches it links.
volatile bool pt_display_suspended = false;

// --- storage functions (no-ops in the sim) ---
void init_storage() {}
void load_settings() {}
void save_settings() {}
void save_tray_weight(int, int) {}
void save_ext_weight() {}
void save_cloud_settings() {}

// --- MQTT (bambu_mqtt.h): commands just log; no real printer ---
void bambu_mqtt_setup() {}
void bambu_mqtt_loop() {}
bool bambu_is_connected() { return true; }
bool bambu_pop_pending_finish(unsigned long *, int *) { return false; }
void bambu_cmd_pause() { Serial.println("[sim] cmd: pause"); }
void bambu_cmd_resume() { Serial.println("[sim] cmd: resume"); }
void bambu_cmd_stop() { Serial.println("[sim] cmd: STOP"); }
void bambu_cmd_set_light(bool on) { Serial.println(on ? "[sim] cmd: light on" : "[sim] cmd: light off"); }
void bambu_cmd_set_speed_level(int level) { Serial.print("[sim] cmd: speed "); Serial.println(level); }
void bambu_cmd_gcode(const char *g) { Serial.print("[sim] cmd: gcode "); Serial.println(g); }
void bambu_cmd_start_project_file(const String &p, bool, const int *) { Serial.print("[sim] cmd: print project "); Serial.println(p); }
void bambu_cmd_start_gcode_file(const String &p) { Serial.print("[sim] cmd: print gcode "); Serial.println(p); }

// --- FTP listing: return a fake but realistic tree ---
bool bambu_ftp_list(const char *path, FtpEntry *out, int max_entries, int *out_count, String *err) {
    (void)err;
    int n = 0;
    String p = path ? String(path) : String("/");
    auto add = [&](const char *name, bool dir, uint32_t size) {
        if (n < max_entries) { out[n].name = name; out[n].is_dir = dir; out[n].size = size; n++; }
    };
    if (p == "/") {
        add("cache", true, 0);
        add("model", true, 0);
        add("timelapse", true, 0);
        add("Maglev Hot Air Balloon.gcode.3mf", false, 1992294);
        add("Benchy.gcode.3mf", false, 843210);
    } else {
        add("Pistachio plier.3mf", false, 1204221);
        add("Keychain.gcode.3mf", false, 342100);
        add("Support remover.gcode.3mf", false, 88213);
    }
    *out_count = n;
    return true;
}

// --- populate the fake state (called once from sim main before building UI) ---
void sim_load_mock_data() {
    g_brightness = 70;
    g_bg_color = 0x101418;
    std::strncpy(g_printer_ip, "192.168.2.27", sizeof(g_printer_ip) - 1);
    std::strncpy(g_printer_serial, "01P00C460103299", sizeof(g_printer_serial) - 1);
    std::strncpy(g_wifi_ssid, "HomeWiFi", sizeof(g_wifi_ssid) - 1);
    g_wifi_status = "Connected";
    g_ip_addr = "192.168.2.33";

    PrinterStatus &s = g_printer_status;
    s.mqtt_connected = true;
    s.have_data = true;
    std::strcpy(s.gcode_state, "RUNNING");
    std::strcpy(s.task_name, "Maglev Hot Air Balloon");
    s.progress_pct = 42;
    s.remaining_min = 87;
    s.layer_num = 84;
    s.total_layers = 200;
    s.nozzle_temp = 220; s.nozzle_target = 220;
    s.bed_temp = 60;     s.bed_target = 60;
    s.chamber_temp = 32;
    s.fan_speed_pct = 80;
    s.light_on = true;
    s.speed_level = 2;

    s.ams_count = 1;
    s.ams[0].present = true;
    s.ams[0].humidity = 2;
    s.ams[0].temp = 28;
    const char *types[4] = {"PLA", "PETG", "PLA", "ABS"};
    uint32_t cols[4] = {0xff5555, 0x55aaff, 0x33dd88, 0xf0f0f0};
    for (int t = 0; t < 4; t++) {
        s.ams[0].trays[t].present = true;
        std::strcpy(s.ams[0].trays[t].type, types[t]);
        s.ams[0].trays[t].color = cols[t];
        s.ams[0].trays[t].remain = 80 - t * 15;
    }
    s.external_spool.present = true;
    std::strcpy(s.external_spool.type, "TPU");
    s.external_spool.color = 0x333333;
    s.external_spool.remain = -1;
    s.active_tray_now = 0;

    // a couple of manual spool weights so the Filament screen shows numbers
    g_tray_capacity_g[0][0] = 1000; g_tray_used_g[0][0] = 250;
    g_tray_capacity_g[0][1] = 1000; g_tray_used_g[0][1] = 600;
    g_ext_capacity_g = 500; g_ext_used_g = 120;
}
