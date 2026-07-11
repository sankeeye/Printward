// LAN HTTP control server for the Android tablet (Android-only, POSIX sockets).
//
// Serves a small mobile web page that mirrors the on-screen Move tab, so the
// printer can be jogged from a phone/PC browser too. A blocking accept loop
// runs on its own thread; button taps hit GET /cmd?a=..&s=.. which validates
// the action and ENQUEUES it. The queue is drained on the main thread by
// webctl_loop() -> move_perform(), so all MQTT publishing stays single-threaded
// (PubSubClient is not thread-safe). GET /status returns the live printer state
// as JSON for the page to poll.
#include "webctl.h"
#include "ui_move.h"
#include "bambu_mqtt.h"

#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <android/log.h>

#define WEBCTL_PORT 8080
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PANDA", __VA_ARGS__)

// --- thread-safe command queue (web thread -> main thread) ---------------
struct QCmd { int code; float step; };
#define QCAP 32
static QCmd g_q[QCAP];
static int g_qh = 0, g_qt = 0;              // head = next read, tail = next write
static SDL_mutex* g_qmtx = nullptr;

static void q_push(int code, float step) {
    if (!g_qmtx) return;
    SDL_LockMutex(g_qmtx);
    int nt = (g_qt + 1) % QCAP;
    if (nt != g_qh) { g_q[g_qt].code = code; g_q[g_qt].step = step; g_qt = nt; }
    SDL_UnlockMutex(g_qmtx);
}

void webctl_loop() {
    if (!g_qmtx) return;
    for (;;) {
        QCmd c;
        SDL_LockMutex(g_qmtx);
        if (g_qh == g_qt) { SDL_UnlockMutex(g_qmtx); break; }
        c = g_q[g_qh];
        g_qh = (g_qh + 1) % QCAP;
        SDL_UnlockMutex(g_qmtx);
        move_perform(c.code, c.step);       // real MQTT publish, main thread
    }
}

// --- request helpers -----------------------------------------------------
static int code_from_name(const char* a) {
    if (!strcmp(a, "xp")) return MOVE_XP;
    if (!strcmp(a, "xm")) return MOVE_XM;
    if (!strcmp(a, "yp")) return MOVE_YP;
    if (!strcmp(a, "ym")) return MOVE_YM;
    if (!strcmp(a, "zp")) return MOVE_ZP;
    if (!strcmp(a, "zm")) return MOVE_ZM;
    if (!strcmp(a, "home")) return MOVE_HOME;
    if (!strcmp(a, "ext")) return MOVE_EEXT;
    if (!strcmp(a, "ret")) return MOVE_ERET;
    if (!strcmp(a, "preheat")) return MOVE_PREHEAT;
    if (!strcmp(a, "cool")) return MOVE_COOL;
    return 0;
}

static bool parse_query(const char* q, const char* key, char* out, int n) {
    out[0] = 0;
    if (!q) return false;
    int kl = (int)strlen(key);
    const char* p = q;
    while (p && *p) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            p += kl + 1;
            int j = 0;
            while (*p && *p != '&' && j < n - 1) out[j++] = *p++;
            out[j] = 0;
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return false;
}

static void send_resp(int fd, const char* status, const char* ctype, const char* body, int blen) {
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
        status, ctype, blen);
    send(fd, hdr, n, 0);
    if (body && blen > 0) send(fd, body, blen, 0);
}

static void build_status(char* out, int n) {
    PrinterStatus& s = g_printer_status;
    char name[80];
    int j = 0;
    for (const char* p = s.task_name; *p && j < (int)sizeof(name) - 1; p++) {
        char ch = *p;
        if (ch == '"' || ch == '\\' || (unsigned char)ch < 0x20) ch = ' ';
        name[j++] = ch;
    }
    name[j] = 0;
    snprintf(out, n,
        "{\"conn\":%s,\"state\":\"%s\",\"pct\":%d,\"layer\":%d,\"total\":%d,"
        "\"nozzle\":%.0f,\"nozzle_t\":%.0f,\"bed\":%.0f,\"bed_t\":%.0f,\"chamber\":%.0f,"
        "\"remain\":%d,\"name\":\"%s\"}",
        s.mqtt_connected ? "true" : "false", s.gcode_state, s.progress_pct,
        s.layer_num, s.total_layers, s.nozzle_temp, s.nozzle_target,
        s.bed_temp, s.bed_target, s.chamber_temp, s.remaining_min, name);
}

// The control page. Self-contained (inline CSS + JS), mobile-first, dark.
static const char PAGE_HTML[] = R"HTML(<!doctype html>
<html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>PandaTouch Control</title>
<style>
:root{--bg:#101418;--panel:#1c2229;--btn:#2c3e50;--txt:#eee;--muted:#9aa4b0}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--txt);font-family:system-ui,Segoe UI,Arial,sans-serif}
header{padding:14px 16px;font-size:20px;font-weight:600;border-bottom:1px solid #2a2f36}
#status{display:flex;flex-wrap:wrap;gap:6px 18px;padding:10px 16px;color:var(--muted);font-size:14px}
#status b{color:var(--txt)}
.step{display:flex;gap:8px;align-items:center;padding:12px 16px 0;color:var(--muted)}
main{padding:16px;display:flex;flex-wrap:wrap;gap:18px;justify-content:center}
.panel{background:var(--panel);border-radius:12px;padding:14px}
.panel h3{margin:0 0 10px;font-size:14px;font-weight:600;color:var(--muted)}
button{background:var(--btn);color:#fff;border:0;border-radius:10px;font-size:18px;
 cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:manipulation}
button:active{filter:brightness(1.35)}
.pad{display:grid;grid-template-columns:repeat(3,76px);grid-template-rows:repeat(3,76px);gap:8px}
.pad button{width:76px;height:76px;font-size:20px}
.home{background:#c0392b;font-size:26px}
.zcol{display:flex;flex-direction:column;gap:10px}
.zcol button{width:104px;height:92px}
.ecol{display:flex;flex-direction:column;gap:10px;min-width:170px}
.ecol .big{height:58px}
.ext{background:#27ae60}.ret{background:#d35400}.ph{background:#e67e22}.cl{background:#2980b9}
.heat{display:flex;gap:8px}.heat button{flex:1;height:46px;font-size:15px}
.step button{width:58px;height:38px;font-size:15px;background:#333}
.step button.sel{background:#3465a4}
#hint{min-height:22px;padding:8px 16px 18px;color:var(--muted);font-size:14px}
</style></head><body>
<header>PandaTouch &mdash; Move</header>
<div id="status">verbinden&hellip;</div>
<div class="step">Stap:
 <button data-s="0.1">0.1</button><button data-s="1" class="sel">1</button><button data-s="10">10</button>
 mm</div>
<main>
 <div class="panel"><h3>X / Y</h3>
  <div class="pad">
   <span></span><button data-a="yp">Y+</button><span></span>
   <button data-a="xm">X-</button><button class="home" data-a="home">&#8962;</button><button data-a="xp">X+</button>
   <span></span><button data-a="ym">Y-</button><span></span>
  </div></div>
 <div class="panel"><h3>Z</h3>
  <div class="zcol"><button data-a="zp">Z+</button><button data-a="zm">Z-</button></div></div>
 <div class="panel"><h3 id="noz">Extruder</h3>
  <div class="ecol">
   <button class="ext big" data-a="ext">Extrude</button>
   <button class="ret big" data-a="ret">Retract</button>
   <div class="heat">
    <button class="ph" data-a="preheat">Preheat</button>
    <button class="cl" data-a="cool">Cooldown</button>
   </div>
  </div></div>
</main>
<div id="hint"></div>
<script>
var step="1";
document.querySelectorAll(".step button").forEach(function(b){b.onclick=function(){
 step=b.dataset.s;
 document.querySelectorAll(".step button").forEach(function(x){x.classList.remove("sel")});
 b.classList.add("sel");};});
document.querySelectorAll("main button").forEach(function(b){b.onclick=function(){
 fetch("/cmd?a="+b.dataset.a+"&s="+step).then(function(r){return r.text();})
  .then(function(t){document.getElementById("hint").textContent=t;})
  .catch(function(){document.getElementById("hint").textContent="geen verbinding";});};});
function poll(){
 fetch("/status").then(function(r){return r.json();}).then(function(s){
  var st=document.getElementById("status");
  if(!s.conn){st.innerHTML="<b>niet verbonden met printer</b>";return;}
  var h="Status: <b>"+(s.state||"-")+"</b>"+
   " &nbsp; Nozzle <b>"+s.nozzle+"/"+s.nozzle_t+"&deg;</b>"+
   " &nbsp; Bed <b>"+s.bed+"/"+s.bed_t+"&deg;</b>";
  if(s.name){h+=" &nbsp; <b>"+s.name+"</b> "+s.pct+"% laag "+s.layer+"/"+s.total;}
  st.innerHTML=h;
  document.getElementById("noz").textContent="Extruder  "+s.nozzle+"/"+s.nozzle_t+"°C";
 }).catch(function(){});
}
setInterval(poll,1500);poll();
</script></body></html>)HTML";

static void handle_conn(int fd) {
    char buf[2048];
    int r = (int)recv(fd, buf, sizeof(buf) - 1, 0);
    if (r <= 0) return;
    buf[r] = 0;

    if (strncmp(buf, "GET ", 4) != 0) { send_resp(fd, "405 Method Not Allowed", "text/plain", "", 0); return; }
    char* path = buf + 4;
    char* sp = strchr(path, ' ');
    if (sp) *sp = 0;
    char* query = strchr(path, '?');
    if (query) { *query = 0; query++; }

    if (!strcmp(path, "/")) {
        send_resp(fd, "200 OK", "text/html; charset=utf-8", PAGE_HTML, (int)strlen(PAGE_HTML));
    } else if (!strcmp(path, "/status")) {
        char js[512];
        build_status(js, sizeof(js));
        send_resp(fd, "200 OK", "application/json", js, (int)strlen(js));
    } else if (!strcmp(path, "/cmd")) {
        char a[16], sbuf[16];
        parse_query(query, "a", a, sizeof(a));
        parse_query(query, "s", sbuf, sizeof(sbuf));
        float step = sbuf[0] ? (float)atof(sbuf) : 1.0f;
        int code = code_from_name(a);
        if (!code) {
            send_resp(fd, "400 Bad Request", "text/plain; charset=utf-8", "onbekend commando", 17);
        } else {
            const char* reason = "";
            if (move_blocked(code, &reason)) {
                send_resp(fd, "200 OK", "text/plain; charset=utf-8", reason, (int)strlen(reason));
            } else {
                q_push(code, step);
                const char* ok = "verstuurd";
                send_resp(fd, "200 OK", "text/plain; charset=utf-8", ok, (int)strlen(ok));
            }
        }
    } else {
        send_resp(fd, "404 Not Found", "text/plain", "", 0);
    }
}

static int server_thread(void*) {
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { LOGI("WEBCTL: socket() failed"); return 0; }
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(WEBCTL_PORT);
    if (bind(ls, (sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGI("WEBCTL: bind :%d failed", WEBCTL_PORT);
        close(ls);
        return 0;
    }
    listen(ls, 4);
    LOGI("WEBCTL: control page on %s", webctl_url());
    for (;;) {
        int fd = accept(ls, nullptr, nullptr);
        if (fd < 0) continue;
        handle_conn(fd);
        close(fd);
    }
    return 0;
}

// Discover the tablet's own LAN IP without getifaddrs() (added only in API 24;
// this device is API 22): open a UDP socket "towards" a remote address and read
// back the source address the kernel picked. No packet is actually sent.
static char g_url[48] = "";
static void detect_url() {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return;
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(53);
    a.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (connect(s, (sockaddr*)&a, sizeof(a)) == 0) {
        sockaddr_in local;
        socklen_t ll = sizeof(local);
        if (getsockname(s, (sockaddr*)&local, &ll) == 0) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
            snprintf(g_url, sizeof(g_url), "http://%s:%d", ip, WEBCTL_PORT);
        }
    }
    close(s);
}

const char* webctl_url() { return g_url; }

void webctl_start() {
    if (g_qmtx) return;                 // already started
    g_qmtx = SDL_CreateMutex();
    detect_url();
    SDL_Thread* th = SDL_CreateThread(server_thread, "webctl", nullptr);
    if (th) SDL_DetachThread(th);
}
