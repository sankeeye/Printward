#include "bambu_mqtt.h"
#include "storage.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

PrinterStatus g_printer_status;

// True when this printer model actually has a chamber-temperature sensor (X1 /
// H2 series). P1P/P1S and A1/A1-mini report only a fixed placeholder value that
// Bambu Studio hides - so we hide it too. Detected from the serial-number prefix.
bool printer_has_chamber_sensor() {
    const char* sn = g_printer_serial;
    if (!sn[0]) return true;                 // serial unknown -> don't hide anything
    static const char* NO_CHAMBER[] = {"01S", "01P", "030", "039"};  // P1P, P1S, A1 mini, A1
    for (const char* p : NO_CHAMBER)
        if (strncmp(sn, p, 3) == 0) return false;
    return true;
}

static WiFiClientSecure g_tls_client;
static PubSubClient g_mqtt(g_tls_client);

static String g_report_topic;
static String g_request_topic;
static unsigned long g_last_reconnect_attempt = 0;
static unsigned long g_last_pushall = 0;   // periodic full-status resync while connected
static bool g_topics_built = false;

static void build_topics() {
    g_report_topic = String("device/") + g_printer_serial + "/report";
    g_request_topic = String("device/") + g_printer_serial + "/request";
    g_topics_built = true;
}

static void publish_json(JsonDocument& doc) {
    if (!g_mqtt.connected()) return;
    String out;
    serializeJson(doc, out);
    g_mqtt.publish(g_request_topic.c_str(), out.c_str());
}

// Parses the alpha-last RRGGBBAA hex color strings used by AMS tray_color
// (e.g. "FF6600FF") down to a 24-bit RGB value for on-screen swatches.
static uint32_t parse_ams_color(const char* hex) {
    if (!hex || strlen(hex) < 6) return 0x808080;
    char buf[7] = {0};
    memcpy(buf, hex, 6);
    return (uint32_t)strtol(buf, nullptr, 16);
}

static void handle_mqtt_message(char* topic, byte* payload, unsigned int length) {
    // ArduinoJson v7 JsonDocument grows on the heap as needed; Bambu's full
    // "pushall" status report can run several KB, partial pushes are much
    // smaller. Filtering isn't used here since we want most top-level fields.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.printf("BAMBU: JSON parse error: %s\n", err.c_str());
        return;
    }

    JsonObject print = doc["print"];
    if (print.isNull()) return; // e.g. "system" acks - nothing to display

    g_printer_status.have_data = true;
    g_printer_status.last_update_ms = millis();

    if (!print["gcode_state"].isNull()) {
        strncpy(g_printer_status.gcode_state, print["gcode_state"] | "", 15);
        g_printer_status.gcode_state[15] = '\0';
    }
    if (!print["subtask_name"].isNull()) {
        strncpy(g_printer_status.task_name, print["subtask_name"] | "", 63);
        g_printer_status.task_name[63] = '\0';
    }
    if (!print["gcode_file"].isNull()) {
        const char* gf = print["gcode_file"] | "";
        if (strcmp(gf, g_printer_status.gcode_file) != 0) {
            strncpy(g_printer_status.gcode_file, gf, sizeof(g_printer_status.gcode_file) - 1);
            g_printer_status.gcode_file[sizeof(g_printer_status.gcode_file) - 1] = '\0';
            Serial.printf("BAMBU: gcode_file = %s\n", g_printer_status.gcode_file);
        }
    }
    if (!print["mc_percent"].isNull()) g_printer_status.progress_pct = print["mc_percent"];
    if (!print["mc_remaining_time"].isNull()) g_printer_status.remaining_min = print["mc_remaining_time"];
    if (!print["layer_num"].isNull()) g_printer_status.layer_num = print["layer_num"];
    if (!print["total_layer_num"].isNull()) g_printer_status.total_layers = print["total_layer_num"];

    if (!print["nozzle_temper"].isNull()) g_printer_status.nozzle_temp = print["nozzle_temper"];
    if (!print["nozzle_target_temper"].isNull()) g_printer_status.nozzle_target = print["nozzle_target_temper"];
    if (!print["bed_temper"].isNull()) g_printer_status.bed_temp = print["bed_temper"];
    if (!print["bed_target_temper"].isNull()) g_printer_status.bed_target = print["bed_target_temper"];
    if (!print["chamber_temper"].isNull()) g_printer_status.chamber_temp = print["chamber_temper"];

    if (!print["cooling_fan_speed"].isNull()) {
        // Bambu reports fan speed as a "0".."15" string-ish gear value in some
        // firmwares; treat as a raw 0-100 percent-ish number defensively.
        int raw = atoi((const char*)(print["cooling_fan_speed"] | "0"));
        g_printer_status.fan_speed_pct = raw > 100 ? 100 : raw;
    }

    if (!print["lights_report"].isNull()) {
        JsonArray lights = print["lights_report"];
        for (JsonObject l : lights) {
            const char* node = l["node"] | "";
            if (strcmp(node, "chamber_light") == 0) {
                const char* mode = l["mode"] | "off";
                g_printer_status.light_on = (strcmp(mode, "on") == 0);
            }
        }
    }

    if (!print["spd_lvl"].isNull()) g_printer_status.speed_level = print["spd_lvl"];

    if (!print["ams"].isNull() && !print["ams"]["tray_now"].isNull()) {
        int raw = atoi((const char*)(print["ams"]["tray_now"] | "255"));
        g_printer_status.active_tray_now = (raw == 255) ? -1 : raw;
    }

    if (!print["ams"].isNull() && !print["ams"]["ams"].isNull()) {
        JsonArray units = print["ams"]["ams"];
        int count = 0;
        for (JsonObject u : units) {
            if (count >= AMS_MAX_UNITS) break;
            AmsUnit& unit = g_printer_status.ams[count];
            unit.present = true;
            // Bambu's 1-5 humidity grade: 1 = most humid (bad) ... 5 = driest (good).
            unit.humidity = atoi((const char*)(u["humidity"] | "-1"));
            // Newer AMS units also report the actual relative humidity as a
            // percentage (lower = drier) - the exact figure Bambu Studio shows.
            unit.humidity_raw = u["humidity_raw"].isNull() ? -1 : atoi((const char*)(u["humidity_raw"] | "-1"));
            unit.temp = atof((const char*)(u["temp"] | "0"));

            JsonArray trays = u["tray"];
            int tcount = 0;
            for (JsonObject t : trays) {
                if (tcount >= AMS_MAX_TRAYS) break;
                AmsTraySlot& slot = unit.trays[tcount];
                const char* type = t["tray_type"] | "";
                if (type[0] == '\0') {
                    slot.present = false;
                    slot.remain = -1;
                } else {
                    slot.present = true;
                    strncpy(slot.type, type, 15);
                    slot.type[15] = '\0';
                    slot.color = parse_ams_color(t["tray_color"] | "808080FF");
                    // Only Bambu-branded spools with an RFID tag report a real
                    // number here; third-party filament reports -1 (unknown).
                    slot.remain = t["remain"].isNull() ? -1 : (int)t["remain"];
                }
                tcount++;
            }
            count++;
        }
        g_printer_status.ams_count = count;
    }

    if (!print["vt_tray"].isNull()) {
        JsonObject vt = print["vt_tray"];
        const char* type = vt["tray_type"] | "";
        if (type[0] == '\0') {
            g_printer_status.external_spool.present = false;
            g_printer_status.external_spool.remain = -1;
        } else {
            g_printer_status.external_spool.present = true;
            strncpy(g_printer_status.external_spool.type, type, 15);
            g_printer_status.external_spool.type[15] = '\0';
            g_printer_status.external_spool.color = parse_ams_color(vt["tray_color"] | "808080FF");
            g_printer_status.external_spool.remain = vt["remain"].isNull() ? -1 : (int)vt["remain"];
        }
    }
}

static void request_pushall() {
    JsonDocument doc;
    doc["pushing"]["sequence_id"] = "0";
    doc["pushing"]["command"] = "pushall";
    publish_json(doc);
}

static bool try_connect() {
    if (strlen(g_printer_ip) == 0 || strlen(g_printer_serial) == 0) return false;
    if (!g_topics_built) build_topics();

    g_mqtt.setServer(g_printer_ip, 8883);
    g_mqtt.setCallback(handle_mqtt_message);
    g_mqtt.setBufferSize(8192);
    g_mqtt.setSocketTimeout(5);
    g_mqtt.setKeepAlive(20);

    String client_id = String("PrintwardBambu-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.printf("BAMBU: Connecting to %s as client %s...\n", g_printer_ip, client_id.c_str());

    bool ok = g_mqtt.connect(client_id.c_str(), "bblp", g_printer_access_code);
    if (!ok) {
        Serial.printf("BAMBU: MQTT connect failed, state=%d\n", g_mqtt.state());
        return false;
    }

    g_mqtt.subscribe(g_report_topic.c_str());
    Serial.printf("BAMBU: Connected, subscribed to %s\n", g_report_topic.c_str());
    request_pushall();
    g_last_pushall = millis();
    return true;
}

void bambu_mqtt_setup() {
    // LAN-only mode uses Bambu's self-signed cert - we're on the same LAN as
    // the printer already, so skip full chain validation.
    g_tls_client.setInsecure();
}

void bambu_mqtt_loop() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (strlen(g_printer_ip) == 0 || strlen(g_printer_serial) == 0) return;

    if (!g_mqtt.connected()) {
        g_printer_status.mqtt_connected = false;
        unsigned long now = millis();
        if (now - g_last_reconnect_attempt > 5000) {
            g_last_reconnect_attempt = now;
            if (try_connect()) {
                g_printer_status.mqtt_connected = true;
            }
        }
        return;
    }

    g_printer_status.mqtt_connected = true;
    g_mqtt.loop();

    // Re-sync the full status periodically. After the first pushall the printer only
    // sends deltas, so a missed update or a hiccup (e.g. Bambu Handy connecting over
    // the cloud) can leave our picture stale forever. A periodic pushall self-heals it.
    unsigned long now = millis();
    if (now - g_last_pushall > 30000) {
        g_last_pushall = now;
        request_pushall();
    }
}

bool bambu_is_connected() {
    return g_printer_status.mqtt_connected;
}

void bambu_mqtt_settings_changed() {
    // Printer IP/serial/access code changed at runtime: drop any live session,
    // force topic rebuild (serial feeds the topic names) and reconnect on the
    // next loop instead of waiting out the 5s backoff.
    if (g_mqtt.connected()) g_mqtt.disconnect();
    g_topics_built = false;
    g_printer_status.mqtt_connected = false;
    g_last_reconnect_attempt = 0;
}

void bambu_cmd_pause() {
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "pause";
    publish_json(doc);
}

void bambu_cmd_resume() {
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "resume";
    publish_json(doc);
}

void bambu_cmd_stop() {
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "stop";
    publish_json(doc);
}

void bambu_cmd_set_light(bool on) {
    JsonDocument doc;
    doc["system"]["sequence_id"] = "0";
    doc["system"]["command"] = "ledctrl";
    doc["system"]["led_node"] = "chamber_light";
    doc["system"]["led_mode"] = on ? "on" : "off";
    doc["system"]["led_on_time"] = 500;
    doc["system"]["led_off_time"] = 500;
    doc["system"]["loop_times"] = 0;
    doc["system"]["interval_time"] = 0;
    publish_json(doc);
    // Optimistically reflect the change immediately; the next status push
    // will correct this if the command didn't actually take.
    g_printer_status.light_on = on;
}

void bambu_cmd_set_speed_level(int level) {
    if (level < 1) level = 1;
    if (level > 4) level = 4;
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "print_speed";
    doc["print"]["param"] = String(level);
    publish_json(doc);
    g_printer_status.speed_level = level;
}

void bambu_cmd_gcode(const char* gcode_line) {
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "gcode_line";
    doc["print"]["param"] = String(gcode_line) + "\n";
    publish_json(doc);
}

// Configure one AMS tray's filament (type/colour/temps) - what the slicer's
// "sync AMS" does. rgb is 0xRRGGBB; tray_color goes out as RRGGBBFF.
void bambu_cmd_set_ams_filament(int ams_id, int tray_id, const char* tray_info_idx,
                                uint32_t rgb, int nozzle_min, int nozzle_max, const char* type) {
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "ams_filament_setting";
    doc["print"]["ams_id"] = ams_id;
    doc["print"]["tray_id"] = tray_id;
    doc["print"]["tray_info_idx"] = tray_info_idx;
    char col[9];
    snprintf(col, sizeof(col), "%06XFF", (unsigned)(rgb & 0xFFFFFF));
    doc["print"]["tray_color"] = col;
    doc["print"]["nozzle_temp_min"] = nozzle_min;
    doc["print"]["nozzle_temp_max"] = nozzle_max;
    doc["print"]["tray_type"] = type;
    publish_json(doc);
    Serial.printf("BAMBU: ams_filament_setting AMS%d T%d %s %s\n", ams_id, tray_id, type, col);
}

void bambu_cmd_start_project_file(const String& ftp_path, bool use_ams, const int ams_mapping[5]) {
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "project_file";
    // Always plate 1 - this firmware doesn't parse the .3mf's internal
    // metadata to know how many plates it has (that'd mean unzipping/XML
    // parsing on-device), so multi-plate project files will only ever print
    // their first plate. Fine for the common single-plate case.
    doc["print"]["param"] = "Metadata/plate_1.gcode";
    doc["print"]["project_id"] = "0";
    doc["print"]["profile_id"] = "0";
    doc["print"]["task_id"] = "0";
    doc["print"]["subtask_id"] = "0";
    doc["print"]["subtask_name"] = "";
    doc["print"]["file"] = "";
    // The SD card is mounted at /sdcard; a file at FTP path "/model/foo.3mf"
    // is "file:///sdcard/model/foo.3mf". (ftp:// and file:///mnt/sdcard both got
    // silently ignored - see the ha-bambulab notes on P1 SD-path formats.)
    String rel_path = ftp_path;
    if (rel_path.startsWith("/")) rel_path = rel_path.substring(1);
    doc["print"]["url"] = "file:///sdcard/" + rel_path;
    doc["print"]["md5"] = "";
    doc["print"]["timelapse"] = true;
    doc["print"]["bed_type"] = "auto";
    doc["print"]["bed_levelling"] = true;
    doc["print"]["flow_cali"] = true;
    doc["print"]["vibration_cali"] = true;
    doc["print"]["layer_inspect"] = true;
    doc["print"]["use_ams"] = use_ams;
    if (use_ams && ams_mapping) {
        JsonArray mapping = doc["print"]["ams_mapping"].to<JsonArray>();
        for (int i = 0; i < 5; i++) mapping.add(ams_mapping[i]);
    } else {
        doc["print"]["ams_mapping"] = "";
    }
    publish_json(doc);
    Serial.println("BAMBU: sent project_file start command for " + ftp_path);
}

void bambu_cmd_start_gcode_file(const String& ftp_path) {
    JsonDocument doc;
    doc["print"]["sequence_id"] = "0";
    doc["print"]["command"] = "gcode_file";
    doc["print"]["param"] = ftp_path;
    publish_json(doc);
    Serial.println("BAMBU: sent gcode_file start command for " + ftp_path);
}
