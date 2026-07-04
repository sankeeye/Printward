#include "webserver.h"
#include "storage.h"
#include "ui_printer.h"
#include "bambu_mqtt.h"
#include "bambu_cloud.h"
#include "webserver_html.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_task_wdt.h>

AsyncWebServer server(80);
static bool webserver_started = false;

// --- Bambu Cloud verification-code "mailbox" -------------------------------
// The device itself can't log in to Bambu Cloud (Cloudflare blocks it - see
// bambu_cloud.h), so the PC relay script does the login. When Bambu demands a
// 6-digit email code, the relay used to prompt for it in its terminal - which
// is invisible once the relay runs hidden in the background. Instead the relay
// now parks a "need a code" flag here and the config web dashboard lets the
// user type the code in a browser; the relay picks it up on its next poll.
// Pure LAN pass-through of a short string - nothing Bambu-specific happens on
// the device, so this lives here rather than in bambu_cloud.*. RAM-only on
// purpose: a pending code is short-lived and must never survive a reboot.
static bool   g_cloud_code_needed = false;  // relay is waiting for a code
static String g_cloud_code_email  = "";     // which account, shown in dashboard
static String g_cloud_submitted_code = "";   // code the user typed, "" until then

void check_wifi_status() {
    static bool was_connected = false;
    static unsigned long last_wifi_check = 0;

    if (millis() - last_wifi_check < 2000) return;
    last_wifi_check = millis();

    bool is_connected = (WiFi.status() == WL_CONNECTED);

    if (is_connected != was_connected) {
        if (is_connected) {
            g_wifi_status = "Connected";
            g_ip_addr = WiFi.localIP().toString();
            Serial.print("WiFi Connected! IP: ");
            Serial.println(g_ip_addr);
            init_webserver();
        } else {
            g_wifi_status = "Disconnected";
            g_ip_addr = "0.0.0.0";
            Serial.println("WiFi Disconnected.");
        }
        was_connected = is_connected;
        update_status_label();
    }
}

static void build_status_json(AsyncWebServerRequest* request) {
    JsonDocument doc;
    PrinterStatus& s = g_printer_status;

    doc["mqtt_connected"] = s.mqtt_connected;
    doc["have_data"] = s.have_data;
    doc["gcode_state"] = s.gcode_state;
    doc["task_name"] = s.task_name;
    doc["progress_pct"] = s.progress_pct;
    doc["remaining_min"] = s.remaining_min;
    doc["nozzle_temp"] = s.nozzle_temp;
    doc["nozzle_target"] = s.nozzle_target;
    doc["bed_temp"] = s.bed_temp;
    doc["bed_target"] = s.bed_target;
    doc["chamber_temp"] = s.chamber_temp;
    doc["fan_speed_pct"] = s.fan_speed_pct;
    doc["light_on"] = s.light_on;
    doc["speed_level"] = s.speed_level;

    JsonArray ams = doc["ams"].to<JsonArray>();
    for (int u = 0; u < s.ams_count && u < AMS_MAX_UNITS; u++) {
        if (!s.ams[u].present) continue;
        JsonObject unit = ams.add<JsonObject>();
        unit["humidity"] = s.ams[u].humidity;
        JsonArray trays = unit["trays"].to<JsonArray>();
        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            JsonObject tray = trays.add<JsonObject>();
            tray["present"] = s.ams[u].trays[t].present;
            if (s.ams[u].trays[t].present) {
                tray["type"] = s.ams[u].trays[t].type;
                char buf[8];
                snprintf(buf, sizeof(buf), "%06X", s.ams[u].trays[t].color);
                tray["color"] = buf;
            }
        }
    }

    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

static void build_config_json(AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["wifi_ssid"] = g_wifi_ssid;
    doc["printer_ip"] = g_printer_ip;
    doc["printer_serial"] = g_printer_serial;
    doc["printer_access_code"] = g_printer_access_code;
    doc["brightness"] = g_brightness;
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void init_webserver() {
    if (webserver_started) return;

    server.on("/api/status", HTTP_GET, build_status_json);
    server.on("/api/config", HTTP_GET, build_config_json);

    server.on("/api/cloud_status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["logged_in"] = bambu_cloud_logged_in();
        doc["email"] = g_cloud_email;
        // Verification-code mailbox state (see globals above) so the dashboard
        // knows when to show the code field and when the relay has taken it.
        doc["code_needed"] = g_cloud_code_needed;
        doc["code_email"] = g_cloud_code_email;
        doc["code_submitted"] = g_cloud_submitted_code.length() > 0;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // Blocking (HTTPS call to Bambu Cloud, roughly 1-2s) - runs synchronously
    // in the request handler like /api/config's restart and the OTA write
    // below already do. This briefly pauses the touchscreen's redraw loop
    // for that couple of seconds (same reasoning as bambu_mqtt_loop()'s
    // reconnect attempts), which is fine for a rare, deliberate action like
    // logging in - not a repeated/rapid one.
    server.on("/api/cloud_login", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("email", true) || !request->hasParam("password", true)) {
            request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Missing email or password\"}");
            return;
        }
        String email = request->getParam("email", true)->value();
        String password = request->getParam("password", true)->value();
        CloudLoginResult result = bambu_cloud_login(email, password);

        const char* result_str = "failed";
        if (result == CLOUD_LOGIN_OK) result_str = "ok";
        else if (result == CLOUD_LOGIN_NEEDS_CODE) result_str = "needs_code";

        JsonDocument doc;
        doc["result"] = result_str;
        doc["message"] = g_cloud_last_error;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/cloud_logout", HTTP_POST, [](AsyncWebServerRequest* request) {
        bambu_cloud_logout();
        request->send(200, "application/json", "{\"result\":\"ok\"}");
    });

    // Called by the PC-side helper script (see docs/bambu_weight_relay.py) -
    // it does the actual Bambu Cloud login/polling from a normal computer,
    // which isn't blocked by Cloudflare the way this device's own attempt
    // is (see bambu_cloud.h), and just reports the resulting print weight
    // here over the LAN. No extra auth beyond being on the local network,
    // consistent with the rest of this API (e.g. /api/config).
    server.on("/api/report_weight", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("weight_g", true)) {
            request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Missing weight_g\"}");
            return;
        }
        float weight_g = request->getParam("weight_g", true)->value().toFloat();
        String task_key = request->hasParam("task_key", true) ? request->getParam("task_key", true)->value() : "";
        bambu_report_print_weight(weight_g, task_key);
        request->send(200, "application/json", "{\"result\":\"ok\"}");
    });

    // Experimental token-bypass test (see bambu_cloud.h): lets a token
    // obtained elsewhere (e.g. by the PC relay script) be pasted in directly,
    // then immediately tries an authenticated /my/tasks call from the device
    // itself to see if that gets through Cloudflare even though /login
    // doesn't. Same blocking-HTTPS-call trade-off as /api/cloud_login.
    server.on("/api/cloud_set_token", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("token", true)) {
            request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Missing token\"}");
            return;
        }
        String token = request->getParam("token", true)->value();
        bambu_cloud_set_token(token);
        request->send(200, "application/json", "{\"result\":\"ok\"}");
    });

    server.on("/api/cloud_test_poll", HTTP_POST, [](AsyncWebServerRequest* request) {
        bool ok = bambu_cloud_poll_now();
        JsonDocument doc;
        doc["result"] = ok ? "ok" : "failed";
        doc["message"] = g_cloud_last_error;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // --- Verification-code mailbox endpoints -------------------------------
    // Called by the PC relay when Bambu asks it for a 6-digit email code:
    // raises the flag (and clears any stale code) so the dashboard shows the
    // input field. Optional "email" is just for display in the dashboard.
    server.on("/api/cloud_need_code", HTTP_POST, [](AsyncWebServerRequest* request) {
        g_cloud_code_needed = true;
        g_cloud_submitted_code = "";
        g_cloud_code_email = request->hasParam("email", true)
            ? request->getParam("email", true)->value() : "";
        request->send(200, "application/json", "{\"result\":\"ok\"}");
    });

    // Called by the browser dashboard: stores the code the user typed. Kept
    // until the relay collects it (cloud_pending_code) and finishes login.
    server.on("/api/cloud_submit_code", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!request->hasParam("code", true)) {
            request->send(400, "application/json", "{\"result\":\"failed\",\"message\":\"Missing code\"}");
            return;
        }
        g_cloud_submitted_code = request->getParam("code", true)->value();
        g_cloud_submitted_code.trim();
        request->send(200, "application/json", "{\"result\":\"ok\"}");
    });

    // Polled by the relay: returns the code the user typed (empty until then).
    server.on("/api/cloud_pending_code", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["needed"] = g_cloud_code_needed;
        doc["code"] = g_cloud_submitted_code;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    // Called by the relay once it has used (or given up on) the code, to reset
    // the mailbox so the dashboard hides the field again.
    server.on("/api/cloud_code_done", HTTP_POST, [](AsyncWebServerRequest* request) {
        g_cloud_code_needed = false;
        g_cloud_submitted_code = "";
        g_cloud_code_email = "";
        request->send(200, "application/json", "{\"result\":\"ok\"}");
    });

    server.on("/api/wifi_scan", HTTP_GET, [](AsyncWebServerRequest* request) {
        // WiFi.scanNetworks(true) runs asynchronously (needed so the async
        // web server's request handler isn't blocked for the ~2-3s a scan
        // takes). First call here kicks it off; the dashboard polls this
        // same endpoint every ~1.5s until results (or an empty list) come
        // back instead of a "still scanning" response.
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) {
            request->send(202, "application/json", "{\"status\":\"scanning\"}");
            return;
        }
        if (n == WIFI_SCAN_FAILED) {
            WiFi.scanNetworks(true /* async */);
            request->send(202, "application/json", "{\"status\":\"scanning\"}");
            return;
        }

        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < n; i++) {
            JsonObject net = arr.add<JsonObject>();
            net["ssid"] = WiFi.SSID(i);
            net["rssi"] = WiFi.RSSI(i);
            net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
        WiFi.scanDelete();

        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {
        bool changed = false;
        if (request->hasParam("wifi_ssid", true)) {
            strncpy(g_wifi_ssid, request->getParam("wifi_ssid", true)->value().c_str(), 31);
            g_wifi_ssid[31] = '\0';
            changed = true;
        }
        if (request->hasParam("wifi_pass", true) && request->getParam("wifi_pass", true)->value().length() > 0) {
            strncpy(g_wifi_pass, request->getParam("wifi_pass", true)->value().c_str(), 63);
            g_wifi_pass[63] = '\0';
            changed = true;
        }
        if (request->hasParam("printer_ip", true)) {
            strncpy(g_printer_ip, request->getParam("printer_ip", true)->value().c_str(), 39);
            g_printer_ip[39] = '\0';
            changed = true;
        }
        if (request->hasParam("printer_serial", true)) {
            strncpy(g_printer_serial, request->getParam("printer_serial", true)->value().c_str(), 31);
            g_printer_serial[31] = '\0';
            changed = true;
        }
        if (request->hasParam("printer_access_code", true)) {
            strncpy(g_printer_access_code, request->getParam("printer_access_code", true)->value().c_str(), 23);
            g_printer_access_code[23] = '\0';
            changed = true;
        }
        if (request->hasParam("brightness", true)) {
            int b = request->getParam("brightness", true)->value().toInt();
            if (b >= 10 && b <= 100) { g_brightness = (uint8_t)b; changed = true; }
        }

        if (changed) save_settings();

        // Printer IP/serial/access code changes require a fresh MQTT
        // connection and a clean re-subscribe topic string; simplest and most
        // reliable is to just restart, same as changing WiFi credentials.
        request->send(200, "text/plain", "OK, restarting...");
        Serial.println("WEB API: Config saved, restarting...");
        delay(500);
        ESP.restart();
    });

    // --- OTA firmware update over WiFi ---
    static bool g_ota_success = false;
    static bool g_ota_failed = false;
    static size_t g_ota_total_size = 0;
    static size_t g_ota_content_length = 0;
    static String g_ota_error_msg = "";

    server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest* request) {
        bool shouldRestart = g_ota_success && !Update.hasError() && !g_ota_failed;

        String response_msg;
        int response_code = 200;

        if (shouldRestart) {
            response_msg = "Update OK. Restarting...";
        } else if (g_ota_failed) {
            response_msg = "Update Failed: " + g_ota_error_msg;
            response_code = 500;
        } else if (Update.hasError()) {
            response_msg = "Update Error";
            response_code = 500;
        } else {
            response_msg = "Update incomplete";
            response_code = 500;
        }

        AsyncWebServerResponse* response = request->beginResponse(response_code, "text/plain", response_msg);
        response->addHeader("Connection", "close");
        request->send(response);

        if (shouldRestart) {
            Serial.println("OTA: SUCCESS - Sending restart command");
            g_ota_progress = -1;
            delay(1000);
            Serial.flush();
            delay(500);
            ESP.restart();
        } else {
            g_ota_success = false;
            g_ota_failed = false;
            g_ota_progress = -1;
        }
    }, [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (!index) {
            g_ota_success = false;
            g_ota_failed = false;
            g_ota_total_size = 0;
            g_ota_content_length = 0;
            g_ota_error_msg = "";

            g_ota_content_length = request->contentLength();
            Serial.printf("OTA: Starting, Content-Length: %zu bytes\n", g_ota_content_length);

            if (g_ota_content_length > 4 * 1024 * 1024) {
                g_ota_error_msg = "File too large";
                g_ota_failed = true;
                return;
            }

            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                g_ota_error_msg = "Cannot begin update";
                g_ota_failed = true;
                Update.printError(Serial);
                return;
            }

            g_ota_screen_requested = true;
            g_ota_progress = 0;
        }

        if (len) {
            if (g_ota_failed) return;

            size_t written = Update.write(data, len);
            g_ota_total_size += written;

            if (g_ota_content_length > 0) {
                g_ota_progress = (g_ota_total_size * 100) / g_ota_content_length;
                if (g_ota_progress > 99) g_ota_progress = 99;
            }

            if (written != len) {
                g_ota_error_msg = "Write error";
                g_ota_failed = true;
                Update.printError(Serial);
                return;
            }

            for (int i = 0; i < 50; i++) {
                esp_task_wdt_reset();
                yield();
                delayMicroseconds(100);
            }
        }

        if (final) {
            if (g_ota_failed) return;

            for (int i = 0; i < 100; i++) {
                esp_task_wdt_reset();
                yield();
                delayMicroseconds(2000);
            }
            delay(2000);

            if (!Update.end(true) || Update.hasError()) {
                g_ota_error_msg = "Update end failed";
                g_ota_failed = true;
                Update.printError(Serial);
                return;
            }

            g_ota_progress = 100;
            g_ota_success = true;
        }
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        String html = FPSTR(INDEX_HTML);
        html.replace("__VERSION__", PANDA_VERSION);
        AsyncWebServerResponse* response = request->beginResponse(200, "text/html; charset=utf-8", html);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    server.begin();
    webserver_started = true;
    Serial.println("Web Server started.");
}
