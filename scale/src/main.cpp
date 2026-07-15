// FilaTrack Scale - firmware for the (SpoolEase) load-cell scale hardware.
//
// An ESP32-S3 + HX711 that reports the weight on the platform to the FilaTrack
// tablet over plain LAN HTTP, and is fully manageable from the tablet: set a
// fixed IP, change WiFi, tare and calibrate - all over HTTP. The tablet is the
// "brain"; this is a dumb sensor with a small web API.
//
// Endpoints:
//   GET /              status/config web page
//   GET /weight        {"g":<grams>,"stable":true|false}
//   GET /info          {"connected":..,"ip":..,"ssid":..,"rssi":..,"static":..}
//   GET /tare          zero the scale (nothing on it)
//   GET /cal?known=G   calibrate with a known weight G (grams) on the platform
//   GET /setwifi?ssid=&pass=   change WiFi, then reboot to reconnect
//   GET /setip?ip=[&gw=&mask=&dns=]   set a fixed IP (gw/mask/dns default to the
//                      current DHCP lease); ?dhcp=1 reverts to DHCP. Reboots.
//
// WiFi credentials are the ones already stored in the ESP (survive a reflash);
// if it can't connect it falls back to an open AP "FilaTrack Scale-setup" so it's
// never lost. Calibration, tare and the static-IP config persist in NVS.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "HX711.h"

// HX711 wiring per the SpoolEase Scale build guide.
#define HX_DOUT 5
#define HX_SCK  4
#define LED_PIN 48            // onboard RGB (WS2812) - SpoolEase "front RGB LED"

HX711 scale;
WebServer server(80);
Preferences prefs;

static uint32_t g_last_req = 0;   // millis of the last request from the tablet

static float g_cal = 1.0f;      // raw units per gram
static long  g_tare = 0;        // raw offset
static uint32_t st_ip = 0, st_gw = 0, st_mask = 0, st_dns = 0;  // 0 = DHCP
static bool g_ap_mode = false;
static uint32_t g_reboot_at = 0;   // millis() to reboot at (0 = none)

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
    return (mx - mn) < 2.0f;
}
static void cors() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

static void handleWeight() {
    g_last_req = millis();
    float g = sample_grams();
    // Auto-zero tracking: when the platform sits stable within a few grams of
    // zero for a few reads, nudge the tare so slow HX711 drift (warm-up/temp)
    // creeps back to 0 like a bench scale. A real roll (hundreds of g) is far
    // outside the window, so it is never tracked away.
    static int zero_hits = 0;
    if (is_stable() && fabsf(g) < 5.0f) {
        if (++zero_hits >= 3) {
            scale.set_offset(scale.get_offset() + (long)(g * g_cal));
            zero_hits = 0;
            g = 0.0f;
        }
    } else {
        zero_hits = 0;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"g\":%.1f,\"stable\":%s}", g, is_stable() ? "true" : "false");
    cors();
    server.send(200, "application/json", buf);
}

static void handleInfo() {
    g_last_req = millis();
    bool c = WiFi.status() == WL_CONNECTED;
    String ip = c ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    char b[240];
    snprintf(b, sizeof(b),
        "{\"connected\":%s,\"ap\":%s,\"ip\":\"%s\",\"ssid\":\"%s\",\"rssi\":%d,\"static\":%s,\"cal\":%.3f}",
        c ? "true" : "false", g_ap_mode ? "true" : "false", ip.c_str(),
        WiFi.SSID().c_str(), (int)WiFi.RSSI(), st_ip ? "true" : "false", g_cal);
    cors();
    server.send(200, "application/json", b);
}

static void handleTare() {
    scale.tare(20);
    g_tare = scale.get_offset();
    prefs.putLong("tare", g_tare);
    cors();
    server.send(200, "text/plain", "getarreerd");
}

static void handleCal() {
    if (!server.hasArg("known")) { cors(); server.send(400, "text/plain", "geef ?known=<gram>"); return; }
    float known = server.arg("known").toFloat();
    if (known <= 0) { cors(); server.send(400, "text/plain", "known moet > 0 zijn"); return; }
    scale.set_scale();
    float raw = scale.get_units(20);
    if (raw == 0) { cors(); server.send(400, "text/plain", "geen gewicht gemeten"); return; }
    g_cal = raw / known;
    scale.set_scale(g_cal);
    prefs.putFloat("cal", g_cal);
    char b[64];
    snprintf(b, sizeof(b), "gekalibreerd: %.3f/g", g_cal);
    cors();
    server.send(200, "text/plain", b);
}

static void handleSetWifi() {
    if (!server.hasArg("ssid")) { cors(); server.send(400, "text/plain", "geef ssid"); return; }
    String ssid = server.arg("ssid");
    String pass = server.hasArg("pass") ? server.arg("pass") : "";
    WiFi.persistent(true);
    WiFi.begin(ssid.c_str(), pass.c_str());   // persisted to NVS
    cors();
    server.send(200, "text/plain", "WiFi opgeslagen, schaal herstart...");
    g_reboot_at = millis() + 800;
}

static void handleSetIp() {
    cors();
    if (server.hasArg("dhcp") && server.arg("dhcp") == "1") {
        prefs.putUInt("ip", 0);
        server.send(200, "text/plain", "DHCP ingesteld, herstart...");
        g_reboot_at = millis() + 600;
        return;
    }
    if (!server.hasArg("ip")) { server.send(400, "text/plain", "geef ?ip=... of ?dhcp=1"); return; }
    IPAddress ip;
    if (!ip.fromString(server.arg("ip"))) { server.send(400, "text/plain", "ongeldig ip"); return; }
    IPAddress gw, mask, dns;
    if (!server.hasArg("gw")   || !gw.fromString(server.arg("gw")))     gw = WiFi.gatewayIP();
    if (!server.hasArg("mask") || !mask.fromString(server.arg("mask"))) mask = WiFi.subnetMask();
    if (!server.hasArg("dns")  || !dns.fromString(server.arg("dns")))   dns = WiFi.dnsIP();
    if ((uint32_t)mask == 0) mask = IPAddress(255, 255, 255, 0);
    if ((uint32_t)gw == 0)   gw = IPAddress(ip[0], ip[1], ip[2], 1);
    if ((uint32_t)dns == 0)  dns = gw;
    prefs.putUInt("ip", (uint32_t)ip);
    prefs.putUInt("gw", (uint32_t)gw);
    prefs.putUInt("mask", (uint32_t)mask);
    prefs.putUInt("dns", (uint32_t)dns);
    char b[80];
    snprintf(b, sizeof(b), "vast IP %s, herstart...", ip.toString().c_str());
    server.send(200, "text/plain", b);
    g_reboot_at = millis() + 800;
}

static const char PAGE[] = R"HTML(<!doctype html><html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>FilaTrack Scale</title>
<style>body{margin:0;background:#0e1216;color:#eceff2;font-family:system-ui,Arial,sans-serif;text-align:center}
h1{font-size:20px;padding:14px;margin:0;border-bottom:1px solid #262c33}h3{color:#93a0ad;margin:22px 0 8px}
#w{font-size:60px;font-weight:700;margin:24px 0 4px}#s{color:#93a0ad;margin-bottom:18px}
button{background:#2c3e50;color:#fff;border:0;border-radius:10px;font-size:16px;padding:12px 18px;margin:5px;cursor:pointer}
input{padding:11px;border-radius:8px;border:1px solid #333b44;background:#161b21;color:#fff;font-size:15px;width:150px;margin:3px}
.m{color:#93a0ad;font-size:13px;min-height:18px;margin:8px}.box{max-width:420px;margin:0 auto;padding:0 12px}</style></head><body>
<h1>FilaTrack Scale</h1><div class="box">
<div id="w">- g</div><div id="s">…</div>
<button onclick="go('/tare')">Tarra (nulstellen)</button>
<div><input id="k" type="number" placeholder="gram" value="500"><button onclick="go('/cal?known='+val('k'))">Kalibreer</button></div>
<div id="m1" class="m"></div>
<h3>Netwerk</h3><div id="info" class="m"></div>
<div><input id="ssid" placeholder="WiFi naam"><input id="pass" type="password" placeholder="wachtwoord">
<button onclick="go('/setwifi?ssid='+enc('ssid')+'&pass='+enc('pass'))">WiFi wijzigen</button></div>
<div><input id="ip" placeholder="vast IP bv 192.168.2.60"><button onclick="go('/setip?ip='+val('ip'))">Vast IP</button>
<button onclick="go('/setip?dhcp=1')">DHCP</button></div>
<div id="m2" class="m"></div></div>
<script>
function val(id){return document.getElementById(id).value;}
function enc(id){return encodeURIComponent(val(id));}
function go(u){fetch(u).then(r=>r.text()).then(t=>{document.getElementById(u.indexOf('wifi')>=0||u.indexOf('ip')>=0?'m2':'m1').textContent=t;});}
function poll(){fetch('/weight').then(r=>r.json()).then(d=>{
 document.getElementById('w').textContent=d.g.toFixed(0)+' g';
 document.getElementById('s').textContent=d.stable?'stabiel':'…meten';}).catch(()=>{});
 fetch('/info').then(r=>r.json()).then(d=>{
 document.getElementById('info').textContent=(d.connected?'verbonden':'AP-modus')+' · '+d.ip+' · '+d.ssid+' · '+d.rssi+'dBm'+(d.static?' · vast IP':'');}).catch(()=>{});}
setInterval(poll,900);poll();
</script></body></html>)HTML";

static void handleRoot() { server.send(200, "text/html; charset=utf-8", PAGE); }

void setup() {
    Serial.begin(115200);
    delay(200);

    prefs.begin("scale", false);
    g_cal  = prefs.getFloat("cal", 1.0f);
    g_tare = prefs.getLong("tare", 0);
    st_ip   = prefs.getUInt("ip", 0);
    st_gw   = prefs.getUInt("gw", 0);
    st_mask = prefs.getUInt("mask", 0);
    st_dns  = prefs.getUInt("dns", 0);

    scale.begin(HX_DOUT, HX_SCK);
    scale.set_scale(g_cal);
    scale.set_offset(g_tare);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    if (st_ip) {
        WiFi.config(IPAddress(st_ip), IPAddress(st_gw), IPAddress(st_mask),
                    IPAddress(st_dns ? st_dns : st_gw));
    }
    WiFi.begin();   // reconnect with the credentials already stored in NVS
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(200);

    if (WiFi.status() != WL_CONNECTED) {
        g_ap_mode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("FilaTrack Scale-setup");
        Serial.print("FilaTrack Scale AP-modus: http://");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.print("FilaTrack Scale klaar. IP: ");
        Serial.println(WiFi.localIP());
        if (MDNS.begin("pandascale")) MDNS.addService("http", "tcp", 80);
    }

    server.on("/", handleRoot);
    server.on("/weight", handleWeight);
    server.on("/info", handleInfo);
    server.on("/tare", handleTare);
    server.on("/cal", handleCal);
    server.on("/setwifi", handleSetWifi);
    server.on("/setip", handleSetIp);
    server.begin();
}

void loop() {
    server.handleClient();

    // Status LED: green while the tablet is talking to us (a request in the last
    // 12 s), red otherwise (tablet off / not on the network).
    static uint32_t led_t = 0;
    if (millis() - led_t > 250) {
        led_t = millis();
        bool linked = (WiFi.status() == WL_CONNECTED) && (millis() - g_last_req < 12000);
        if (linked) neopixelWrite(LED_PIN, 0, 30, 0);   // green
        else        neopixelWrite(LED_PIN, 40, 0, 0);   // red
    }

    if (g_reboot_at && (int32_t)(millis() - g_reboot_at) >= 0) { delay(50); ESP.restart(); }
    delay(2);
}
