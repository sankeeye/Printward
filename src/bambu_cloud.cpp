#include "bambu_cloud.h"
#include "storage.h"
#include "bambu_mqtt.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static const char* CLOUD_HOST = "https://api.bambulab.com";
static unsigned long g_last_poll_ms = 0;
String g_cloud_last_error = "";

void bambu_cloud_setup() {
    // No persistent client/connection needed - HTTPClient opens a fresh TLS
    // connection per request, which is fine at the "once a minute at most"
    // rate this is used at.
}

bool bambu_cloud_logged_in() {
    return g_cloud_access_token.length() > 0;
}

void bambu_cloud_logout() {
    g_cloud_access_token = "";
    g_cloud_last_task_key = "";
    save_cloud_settings();
}

CloudLoginResult bambu_cloud_login(const String& email, const String& password) {
    g_cloud_last_error = "";

    if (WiFi.status() != WL_CONNECTED) {
        g_cloud_last_error = "Device has no WiFi connection right now";
        return CLOUD_LOGIN_FAILED;
    }

    WiFiClientSecure client;
    // Bambu Cloud presents a normal publicly-trusted certificate (unlike the
    // printer's self-signed LAN one), but this device has no root CA bundle
    // loaded, so full chain validation isn't available here either - same
    // trust trade-off already accepted for the LAN MQTT link.
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    String url = String(CLOUD_HOST) + "/v1/user-service/user/login";
    if (!http.begin(client, url)) {
        g_cloud_last_error = "Could not start HTTPS request (bad URL/TLS setup)";
        return CLOUD_LOGIN_FAILED;
    }
    http.addHeader("Content-Type", "application/json");
    // Some cloud APIs quietly reject requests with no recognizable client -
    // a plain User-Agent costs nothing to add and rules that out.
    http.addHeader("User-Agent", "FilaTrackBambuPanel/1.0");

    JsonDocument body;
    body["account"] = email;
    body["password"] = password;
    String out;
    serializeJson(body, out);

    int code = http.POST(out);
    if (code <= 0) {
        // Negative return = a transport-level failure (DNS, TCP connect, TLS
        // handshake, timeout) - never reached Bambu at all. HTTPClient's
        // errorToString() turns that into an actual readable reason instead
        // of just a mystery number.
        g_cloud_last_error = "Connection to api.bambulab.com failed: " + HTTPClient::errorToString(code) + " (" + String(code) + ")";
        Serial.printf("CLOUD: login request failed: %s\n", g_cloud_last_error.c_str());
        http.end();
        return CLOUD_LOGIN_FAILED;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        // Not valid JSON at all - Bambu's own API always replies in JSON
        // (see the docs' error-response shape), so this usually means
        // something in front of it (a WAF/bot-protection page, a captive
        // portal, etc.) intercepted the request instead. Including a snippet
        // of the actual body lets us tell those apart without serial access.
        String snippet = payload.substring(0, 200);
        snippet.replace("\n", " ");
        g_cloud_last_error = "HTTP " + String(code) + ", non-JSON response: \"" + snippet + "\"";
        Serial.printf("CLOUD: login response parse error (HTTP %d): %s\n", code, err.c_str());
        Serial.println("CLOUD: raw response: " + payload);
        return CLOUD_LOGIN_FAILED;
    }

    if (code != 200) {
        const char* msg = doc["error"] | doc["message"] | "no error detail in response";
        g_cloud_last_error = "HTTP " + String(code) + ": " + String(msg);
        Serial.printf("CLOUD: login failed, HTTP %d: %s\n", code, msg);
        return CLOUD_LOGIN_FAILED;
    }

    const char* login_type = doc["loginType"] | "";
    if (login_type[0] != '\0') {
        // e.g. "verifyCode" - Bambu wants a one-time code before it'll issue
        // a token. There's no confirmed local endpoint to trigger sending
        // that code, so surface this distinctly rather than pretending it
        // failed outright.
        g_cloud_last_error = String("Bambu requires a verification code (loginType=") + login_type + ")";
        return CLOUD_LOGIN_NEEDS_CODE;
    }

    const char* token = doc["accessToken"] | "";
    if (token[0] == '\0') {
        g_cloud_last_error = "HTTP 200 but no accessToken in the response - unexpected API shape";
        Serial.println("CLOUD: raw response: " + payload);
        return CLOUD_LOGIN_FAILED;
    }

    g_cloud_access_token = String(token);
    g_cloud_email = email;
    save_cloud_settings();
    Serial.println("CLOUD: Login OK, access token saved");
    return CLOUD_LOGIN_OK;
}

// Attributes a finished print's weight to whichever tray was feeding at the
// time - see active_tray_now / bambu_pop_pending_finish() in bambu_mqtt.h.
static void apply_consumed_weight(int tray_now, float weight_g) {
    if (weight_g <= 0) return;
    if (tray_now == 254) {
        g_ext_used_g += weight_g;
        save_ext_weight();
        Serial.printf("CLOUD: Attributed %.1fg to external spool\n", weight_g);
    } else if (tray_now >= 0 && tray_now < AMS_MAX_UNITS * AMS_MAX_TRAYS) {
        int u = tray_now / AMS_MAX_TRAYS;
        int t = tray_now % AMS_MAX_TRAYS;
        g_tray_used_g[u][t] += weight_g;
        save_tray_weight(u, t);
        Serial.printf("CLOUD: Attributed %.1fg to AMS %d tray %d\n", weight_g, u + 1, t + 1);
    } else {
        Serial.printf("CLOUD: %.1fg used but no tray was recorded as active - not attributed\n", weight_g);
    }
}

static bool poll_tasks() {
    if (strlen(g_printer_serial) == 0) {
        g_cloud_last_error = "No printer serial configured yet";
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    String url = String(CLOUD_HOST) + "/v1/user-service/my/tasks?deviceId=" + String(g_printer_serial) + "&limit=5";
    if (!http.begin(client, url)) {
        g_cloud_last_error = "Could not start HTTPS request";
        return false;
    }
    http.addHeader("Authorization", "Bearer " + g_cloud_access_token);

    int code = http.GET();
    if (code <= 0) {
        g_cloud_last_error = "Connection to api.bambulab.com failed: " + HTTPClient::errorToString(code) + " (" + String(code) + ")";
        Serial.printf("CLOUD: /my/tasks request failed: %s\n", g_cloud_last_error.c_str());
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (code != 200) {
        if (code == 401) {
            g_cloud_last_error = "HTTP 401: token rejected (expired?) - clearing, please log in again";
            Serial.println("CLOUD: token rejected (expired?) - clearing, please log in again via the dashboard");
            bambu_cloud_logout();
        } else {
            String snippet = payload.substring(0, 200);
            snippet.replace("\n", " ");
            g_cloud_last_error = "HTTP " + String(code) + ": \"" + snippet + "\"";
            Serial.printf("CLOUD: /my/tasks request failed, HTTP %d\n", code);
        }
        return false;
    }

    // Only pull the handful of fields we actually need - the full response
    // also carries thumbnail/cover URLs etc. that would otherwise bloat the
    // heap for no reason.
    JsonDocument filter;
    filter["hits"][0]["endTime"] = true;
    filter["hits"][0]["startTime"] = true;
    filter["hits"][0]["weight"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (err) {
        String snippet = payload.substring(0, 200);
        snippet.replace("\n", " ");
        g_cloud_last_error = "HTTP 200 but couldn't parse the response: \"" + snippet + "\"";
        Serial.printf("CLOUD: /my/tasks parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray hits = doc["hits"];
    if (hits.isNull()) {
        g_cloud_last_error = "OK - connected, but response had no task list";
        return true;
    }

    // Collect completed tasks newer than the last one we've already applied,
    // oldest first, so they get matched against the pending-finish queue
    // (FIFO) in the order they actually happened.
    struct Completed { String endTime; float weight; };
    Completed newer[5];
    int newer_count = 0;

    for (JsonObject hit : hits) {
        const char* end_time = hit["endTime"] | "";
        const char* start_time = hit["startTime"] | "";
        float weight = hit["weight"] | 0.0f;
        if (end_time[0] == '\0' || weight <= 0) continue;
        // "if endTime is within a minute of startTime, that means the file
        // is currently printing" (still-running placeholder, not a real
        // finished task yet) - per Bambu Cloud's own task list semantics.
        // Skip those.
        // (Comparing ISO8601 strings lexicographically is good enough for
        // "is this newer than the last one we processed".)
        if (String(end_time) <= g_cloud_last_task_key) continue;
        if (newer_count < 5) {
            newer[newer_count].endTime = end_time;
            newer[newer_count].weight = weight;
            newer_count++;
        }
        (void)start_time;
    }

    if (newer_count == 0) {
        g_cloud_last_error = "OK - connected, no new finished prints";
        return true;
    }

    // Bambu's list is newest-first; walk it backwards to apply oldest-first.
    String latest_key = g_cloud_last_task_key;
    for (int i = newer_count - 1; i >= 0; i--) {
        unsigned long finish_ms;
        int tray_now;
        if (bambu_pop_pending_finish(&finish_ms, &tray_now)) {
            apply_consumed_weight(tray_now, newer[i].weight);
        } else {
            Serial.printf("CLOUD: %.1fg reported but no matching local finish event - not attributed\n", newer[i].weight);
        }
        if (newer[i].endTime > latest_key) latest_key = newer[i].endTime;
    }

    g_cloud_last_task_key = latest_key;
    save_cloud_settings();
    g_cloud_last_error = "OK - connected, applied " + String(newer_count) + " new print(s)";
    return true;
}

void bambu_cloud_loop() {
    if (!bambu_cloud_logged_in()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    unsigned long now = millis();
    if (now - g_last_poll_ms < 60000) return;
    g_last_poll_ms = now;

    poll_tasks();
}

void bambu_cloud_set_token(const String& token) {
    g_cloud_access_token = token;
    save_cloud_settings();
}

bool bambu_cloud_poll_now() {
    g_last_poll_ms = millis();
    return poll_tasks();
}

void bambu_report_print_weight(float weight_g, const String& task_key) {
    if (weight_g <= 0) return;

    // Same dedupe field/logic as poll_tasks() uses for its own (blocked)
    // direct-from-device polling - a PC-side helper script and the device's
    // own (currently non-functional, Cloudflare-blocked) polling won't double
    // count the same print even if both were somehow active at once.
    if (task_key.length() > 0 && task_key <= g_cloud_last_task_key) {
        Serial.printf("CLOUD: report_weight - task %s already applied, ignoring\n", task_key.c_str());
        return;
    }

    unsigned long finish_ms;
    int tray_now;
    if (bambu_pop_pending_finish(&finish_ms, &tray_now)) {
        apply_consumed_weight(tray_now, weight_g);
    } else {
        Serial.printf("CLOUD: report_weight - %.1fg reported but no matching local finish event - not attributed\n", weight_g);
    }

    if (task_key.length() > 0 && task_key > g_cloud_last_task_key) {
        g_cloud_last_task_key = task_key;
        save_cloud_settings();
    }
}
