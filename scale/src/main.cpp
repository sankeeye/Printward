// PandaScale - firmware for the (SpoolEase) load-cell scale hardware.
//
// An ESP32-S3 + HX711 that reports the weight on the platform to the PandaTouch
// tablet over plain LAN HTTP. The tablet is the "brain": it polls GET /weight
// when weighing a spool. This firmware is deliberately a dumb sensor - no NFC,
// no database, no printer MQTT - just a calibrated scale with a tiny web API.
//
// Endpoints:
//   GET /            calibration/status web page (live weight, tare, calibrate)
//   GET /weight      {"g":<grams>,"raw":<long>,"stable":true|false}
//   GET /tare        zero the scale with nothing (or the empty jig) on it
//   GET /cal?known=G calibrate: put a known weight G (grams) on, then call this
//
// WiFi is configured once via a captive portal ("PandaScale-setup") - no
// credentials are hard-coded. Calibration + tare persist in NVS.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include "HX711.h"

// HX711 wiring per the SpoolEase Scale build guide.
#define HX_DOUT 5
#define HX_SCK  4

HX711 scale;
WebServer server(80);
Preferences prefs;

static float g_cal = 1.0f;      // raw units per gram (1.0 = uncalibrated)
static long  g_tare = 0;        // raw offset captured at tare

// Small ring of recent readings, to report whether the value has settled.
static float g_recent[6] = {0};
static int   g_ri = 0;

static float sample_grams() {
    float g = 0;
    if (scale.wait_ready_timeout(300)) g = scale.get_units(3);
    g_recent[g_ri] = g;
    g_ri = (g_ri + 1) % 6;
    return g;
}

static bool is_stable() {
    float mn = 1e9f, mx = -1e9f;
    for (int i = 0; i < 6; i++) { if (g_recent[i] < mn) mn = g_recent[i]; if (g_recent[i] > mx) mx = g_recent[i]; }
    return (mx - mn) < 2.0f;   // within 2 g across the last few reads
}

static void cors() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

static void handleWeight() {
    float g = sample_grams();
    long raw = scale.is_ready() ? scale.read() : 0;
    char buf[112];
    snprintf(buf, sizeof(buf), "{\"g\":%.1f,\"raw\":%ld,\"stable\":%s}", g, raw, is_stable() ? "true" : "false");
    cors();
    server.send(200, "application/json", buf);
}

static void handleTare() {
    scale.tare(20);
    g_tare = scale.get_offset();
    prefs.putLong("tare", g_tare);
    cors();
    server.send(200, "text/plain", "getarreerd");
}

static void handleCal() {
    if (!server.hasArg("known")) { server.send(400, "text/plain", "geef ?known=<gram>"); return; }
    float known = server.arg("known").toFloat();
    if (known <= 0) { server.send(400, "text/plain", "known moet > 0 zijn"); return; }
    scale.set_scale();                    // scale factor = 1 -> get_units returns raw-minus-tare
    float raw = scale.get_units(20);
    if (raw == 0) { server.send(400, "text/plain", "geen gewicht gemeten"); return; }
    g_cal = raw / known;                  // raw units per gram
    scale.set_scale(g_cal);
    prefs.putFloat("cal", g_cal);
    char b[64];
    snprintf(b, sizeof(b), "gekalibreerd: %.3f/g", g_cal);
    cors();
    server.send(200, "text/plain", b);
}

static const char PAGE[] = R"HTML(<!doctype html><html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>PandaScale</title>
<style>body{margin:0;background:#0e1216;color:#eceff2;font-family:system-ui,Arial,sans-serif;text-align:center}
h1{font-size:20px;padding:14px;margin:0;border-bottom:1px solid #262c33}
#w{font-size:64px;font-weight:700;margin:30px 0 6px}#s{color:#93a0ad;margin-bottom:24px}
button{background:#2c3e50;color:#fff;border:0;border-radius:10px;font-size:17px;padding:14px 22px;margin:6px;cursor:pointer}
input{padding:12px;border-radius:8px;border:1px solid #333b44;background:#161b21;color:#fff;font-size:16px;width:120px}
.muted{color:#93a0ad;font-size:13px;margin-top:26px;padding:0 16px;word-break:break-all}</style></head><body>
<h1>PandaScale</h1>
<div id="w">– g</div><div id="s">…</div>
<button onclick="fetch('/tare').then(r=>r.text()).then(t=>msg(t))">Tarra (nulstellen)</button><br>
<input id="k" type="number" placeholder="gram" value="500">
<button onclick="fetch('/cal?known='+document.getElementById('k').value).then(r=>r.text()).then(t=>msg(t))">Kalibreer</button>
<div id="m" class="muted"></div>
<div class="muted">Zet een bekend gewicht op de schaal, vul de grammen in en tik Kalibreer.<br>
Tablet-URL: <b id="u"></b>/weight</div>
<script>
function msg(t){document.getElementById('m').textContent=t;}
document.getElementById('u').textContent=location.host;
function poll(){fetch('/weight').then(r=>r.json()).then(d=>{
 document.getElementById('w').textContent=d.g.toFixed(0)+' g';
 document.getElementById('s').textContent=d.stable?'stabiel':'…meten';
}).catch(()=>{});}
setInterval(poll,700);poll();
</script></body></html>)HTML";

static void handleRoot() { server.send(200, "text/html; charset=utf-8", PAGE); }

void setup() {
    Serial.begin(115200);
    delay(200);

    prefs.begin("scale", false);
    g_cal = prefs.getFloat("cal", 1.0f);
    g_tare = prefs.getLong("tare", 0);

    scale.begin(HX_DOUT, HX_SCK);
    scale.set_scale(g_cal);
    scale.set_offset(g_tare);

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    if (!wm.autoConnect("PandaScale-setup")) {
        Serial.println("WiFi config timeout, herstart");
        delay(1000);
        ESP.restart();
    }

    if (MDNS.begin("pandascale")) MDNS.addService("http", "tcp", 80);

    server.on("/", handleRoot);
    server.on("/weight", handleWeight);
    server.on("/tare", handleTare);
    server.on("/cal", handleCal);
    server.begin();

    Serial.print("PandaScale klaar. IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("Test: http://pandascale.local/  of  http://<ip>/weight");
}

void loop() {
    server.handleClient();
    delay(2);
}
