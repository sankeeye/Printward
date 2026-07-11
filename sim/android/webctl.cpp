// LAN HTTP control server for the Android tablet (Android-only, POSIX sockets).
//
// Serves a full "copy of the tablet" as a single-page web app: Dashboard +
// controls, Filament/AMS, Files (FTP browser), Move (jog) and Settings. It
// exposes the same in-memory state (g_printer_status, storage globals) and the
// same commands as the on-screen UI.
//
// A blocking accept loop runs on its own thread. Read-only endpoints (/status,
// /files) answer straight from that thread; anything that publishes MQTT or
// touches LVGL/storage only ENQUEUES a request that webctl_loop() drains on the
// MAIN thread, keeping PubSubClient single-threaded (it is not thread-safe).
#include "webctl.h"
#include "ui_move.h"
#include "scale_client.h"     // g_scale_ip (exposed to the web Scale tab)
#include "filament_track.h"   // filament_remaining(), g_low_threshold_g
#include "spool_db.h"         // spool library + load-to-AMS
#include "notify.h"           // g_ntfy_topic
#include "stats.h"            // g_stat_prints, g_stat_grams
#include "history.h"          // recent-print log + total cost
#include "ui_screensaver.h"   // g_screensaver_3d, screensaver_view_changed()
#include "bambu_mqtt.h"
#include "bambu_ftp.h"
#include "storage.h"
#include "pt/pt_display.h"

#include <SDL.h>
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <android/log.h>

#define WEBCTL_PORT 8080
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PANDA", __VA_ARGS__)

extern bool g_screensaver_3d;

// --- thread-safe request queue (web thread -> main thread) ---------------
enum QKind { Q_MOVE = 1, Q_CTL, Q_CFG, Q_START, Q_SPOOL_SAVE, Q_SPOOL_DEL, Q_SPOOL_LOAD,
             Q_EMPTY_SAVE, Q_EMPTY_DEL, Q_SPOOL_BULK };
enum CtlCode { CTL_PAUSE = 1, CTL_RESUME, CTL_STOP, CTL_LIGHT, CTL_FAN, CTL_SPEED };

struct QCmd {
    int kind;
    int code;       // MOVE_* for Q_MOVE, CTL_* for Q_CTL, idx for Q_SPOOL_*
    float step;     // step (mm) for Q_MOVE, value for Q_CTL, slot for Q_SPOOL_LOAD
    char arg[512];  // query for Q_CFG / Q_SPOOL_SAVE, path for Q_START
};
#define QCAP 32
static QCmd g_q[QCAP];
static int g_qh = 0, g_qt = 0;
static SDL_mutex* g_qmtx = nullptr;
static SDL_mutex* g_ftpmtx = nullptr;   // serialize FTP listings

static void q_push(int kind, int code, float step, const char* arg) {
    if (!g_qmtx) return;
    SDL_LockMutex(g_qmtx);
    int nt = (g_qt + 1) % QCAP;
    if (nt != g_qh) {
        g_q[g_qt].kind = kind;
        g_q[g_qt].code = code;
        g_q[g_qt].step = step;
        if (arg) { strncpy(g_q[g_qt].arg, arg, sizeof(g_q[g_qt].arg) - 1); g_q[g_qt].arg[sizeof(g_q[g_qt].arg) - 1] = 0; }
        else g_q[g_qt].arg[0] = 0;
        g_qt = nt;
    }
    SDL_UnlockMutex(g_qmtx);
}

// --- small parsing / encoding helpers ------------------------------------
static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// URL-decode src into dst (%xx and '+'), NUL-terminated, bounded by n.
static void url_decode(const char* src, char* dst, int n) {
    int j = 0;
    for (const char* p = src; *p && j < n - 1; p++) {
        if (*p == '%' && p[1] && p[2]) {
            int hi = hexval(p[1]), lo = hexval(p[2]);
            if (hi >= 0 && lo >= 0) { dst[j++] = (char)((hi << 4) | lo); p += 2; continue; }
        }
        dst[j++] = (*p == '+') ? ' ' : *p;
    }
    dst[j] = 0;
}

// Extract & URL-decode query parameter `key` from `q` ("a=1&b=2"). false if absent.
static bool parse_query(const char* q, const char* key, char* out, int n) {
    out[0] = 0;
    if (!q) return false;
    int kl = (int)strlen(key);
    const char* p = q;
    while (p && *p) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            const char* v = p + kl + 1;
            char raw[256];
            int j = 0;
            while (*v && *v != '&' && j < (int)sizeof(raw) - 1) raw[j++] = *v++;
            raw[j] = 0;
            url_decode(raw, out, n);
            return true;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return false;
}

// Append a JSON-escaped copy of `in` to out[*p..] (bounded).
static void json_escape(const char* in, char* out, int n) {
    int j = 0;
    for (const char* s = in; *s && j < n - 2; s++) {
        char c = *s;
        if (c == '"' || c == '\\') { if (j < n - 3) out[j++] = '\\'; out[j++] = c; }
        else if ((unsigned char)c < 0x20) out[j++] = ' ';
        else out[j++] = c;
    }
    out[j] = 0;
}

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

static void send_resp(int fd, const char* status, const char* ctype, const char* body, int blen) {
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
        status, ctype, blen);
    send(fd, hdr, n, 0);
    if (body && blen > 0) send(fd, body, blen, 0);
}

// --- main-thread queue drain ---------------------------------------------
void webctl_loop() {
    if (!g_qmtx) return;
    for (;;) {
        QCmd c;
        SDL_LockMutex(g_qmtx);
        if (g_qh == g_qt) { SDL_UnlockMutex(g_qmtx); break; }
        c = g_q[g_qh];
        g_qh = (g_qh + 1) % QCAP;
        SDL_UnlockMutex(g_qmtx);

        switch (c.kind) {
            case Q_MOVE:
                move_perform(c.code, c.step);
                break;
            case Q_CTL:
                switch (c.code) {
                    case CTL_PAUSE:  bambu_cmd_pause(); break;
                    case CTL_RESUME: bambu_cmd_resume(); break;
                    case CTL_STOP:   bambu_cmd_stop(); break;
                    case CTL_LIGHT:  bambu_cmd_set_light(c.step != 0); break;
                    case CTL_FAN:    bambu_cmd_gcode(c.step != 0 ? "M106 P1 S255" : "M106 P1 S0"); break;
                    case CTL_SPEED:  bambu_cmd_set_speed_level((int)c.step); break;
                }
                break;
            case Q_CFG: {
                char v[64];
                bool reconnect = false, viewchg = false;
                if (parse_query(c.arg, "ip", v, sizeof(v)) && v[0]) {
                    if (strcmp(v, g_printer_ip)) reconnect = true;
                    strncpy(g_printer_ip, v, sizeof(g_printer_ip) - 1); g_printer_ip[sizeof(g_printer_ip) - 1] = 0;
                }
                if (parse_query(c.arg, "serial", v, sizeof(v)) && v[0]) {
                    if (strcmp(v, g_printer_serial)) reconnect = true;
                    strncpy(g_printer_serial, v, sizeof(g_printer_serial) - 1); g_printer_serial[sizeof(g_printer_serial) - 1] = 0;
                }
                if (parse_query(c.arg, "code", v, sizeof(v)) && v[0]) {
                    strncpy(g_printer_access_code, v, sizeof(g_printer_access_code) - 1);
                    g_printer_access_code[sizeof(g_printer_access_code) - 1] = 0;
                    reconnect = true;
                }
                if (parse_query(c.arg, "view3d", v, sizeof(v))) {
                    bool nv = (v[0] == '1');
                    if (nv != g_screensaver_3d) { g_screensaver_3d = nv; viewchg = true; }
                }
                if (parse_query(c.arg, "scale_ip", v, sizeof(v)) && v[0]) {
                    strncpy(g_scale_ip, v, sizeof(g_scale_ip) - 1);
                    g_scale_ip[sizeof(g_scale_ip) - 1] = 0;
                }
                if (parse_query(c.arg, "ntfy", v, sizeof(v))) {   // may be cleared to empty
                    strncpy(g_ntfy_topic, v, sizeof(g_ntfy_topic) - 1);
                    g_ntfy_topic[sizeof(g_ntfy_topic) - 1] = 0;
                }
                if (parse_query(c.arg, "bri", v, sizeof(v)) && v[0]) {
                    int b = atoi(v); if (b < 5) b = 5; if (b > 100) b = 100;
                    g_brightness = (uint8_t)b;
                    pt_set_backlight(g_brightness, false);
                }
                save_settings();
                if (viewchg) screensaver_view_changed();
                if (reconnect) bambu_mqtt_settings_changed();
                break;
            }
            case Q_START: {
                String pth(c.arg);
                const char* ext = strrchr(c.arg, '.');
                if (ext && strcasecmp(ext, ".gcode") == 0) {
                    bambu_cmd_start_gcode_file(pth);
                } else {
                    int m[5] = {-1, -1, -1, -1, -1};
                    bambu_cmd_start_project_file(pth, false, m);
                }
                break;
            }
            case Q_SPOOL_SAVE: {
                Spool s; memset(&s, 0, sizeof(s));
                char v[96];
                parse_query(c.arg, "name", s.name, sizeof(s.name));
                parse_query(c.arg, "material", s.material, sizeof(s.material));
                parse_query(c.arg, "code", s.code, sizeof(s.code));
                parse_query(c.arg, "note", s.note, sizeof(s.note));
                if (parse_query(c.arg, "color", v, sizeof(v))) s.color = (uint32_t)strtoul(v[0] == '#' ? v + 1 : v, nullptr, 16);
                if (parse_query(c.arg, "rem", v, sizeof(v))) s.remaining_g = (float)atof(v);
                if (parse_query(c.arg, "empty", v, sizeof(v))) s.empty_g = (float)atof(v);
                if (parse_query(c.arg, "nmin", v, sizeof(v))) s.nmin = atoi(v);
                if (parse_query(c.arg, "nmax", v, sizeof(v))) s.nmax = atoi(v);
                if (parse_query(c.arg, "price", v, sizeof(v))) s.price_kg = (float)atof(v);
                spool_upsert(c.code, s);          // c.code = idx (-1 = add)
                break;
            }
            case Q_SPOOL_DEL:  spool_delete(c.code); break;
            case Q_SPOOL_LOAD: spool_load_to_slot(c.code, (int)c.step); break;
            case Q_EMPTY_SAVE: {
                EmptySpool e; memset(&e, 0, sizeof(e));
                char v[24];
                parse_query(c.arg, "name", e.name, sizeof(e.name));
                if (parse_query(c.arg, "weight", v, sizeof(v))) e.weight_g = (float)atof(v);
                empty_upsert(c.code, e);
                break;
            }
            case Q_EMPTY_DEL: empty_delete(c.code); break;
            case Q_SPOOL_BULK: {
                char act[16], v[64], idxs[300];
                parse_query(c.arg, "act", act, sizeof(act));
                parse_query(c.arg, "v", v, sizeof(v));
                parse_query(c.arg, "idx", idxs, sizeof(idxs));
                int ids[80], n = 0;
                for (char* p = idxs; *p && n < 80; ) {
                    ids[n++] = atoi(p);
                    char* comma = strchr(p, ',');
                    if (!comma) break;
                    p = comma + 1;
                }
                if (!strcmp(act, "del")) {
                    for (int a = 0; a < n; a++)          // delete high->low so indices stay valid
                        for (int b = a + 1; b < n; b++)
                            if (ids[b] > ids[a]) { int t = ids[a]; ids[a] = ids[b]; ids[b] = t; }
                    for (int a = 0; a < n; a++) spool_delete(ids[a]);
                } else if (!strcmp(act, "color")) {
                    uint32_t col = (uint32_t)strtoul(v[0] == '#' ? v + 1 : v, nullptr, 16);
                    for (int a = 0; a < n; a++) if (ids[a] >= 0 && ids[a] < g_spool_count) g_spools[ids[a]].color = col;
                    spool_db_save();
                } else if (!strcmp(act, "weight")) {
                    float g = (float)atof(v);
                    for (int a = 0; a < n; a++) if (ids[a] >= 0 && ids[a] < g_spool_count) g_spools[ids[a]].remaining_g = g;
                    spool_db_save();
                } else if (!strcmp(act, "material")) {
                    for (int a = 0; a < n; a++) if (ids[a] >= 0 && ids[a] < g_spool_count) {
                        strncpy(g_spools[ids[a]].material, v, sizeof(g_spools[0].material) - 1);
                        g_spools[ids[a]].material[sizeof(g_spools[0].material) - 1] = 0;
                    }
                    spool_db_save();
                }
                break;
            }
        }
    }
}

// --- JSON builders -------------------------------------------------------
static void build_status(char* o, int n) {
    PrinterStatus& s = g_printer_status;
    int p = 0;
    char name[96];
    json_escape(s.task_name, name, sizeof(name));
    p += snprintf(o + p, n - p,
        "{\"conn\":%s,\"state\":\"%s\",\"name\":\"%s\",\"pct\":%d,\"layer\":%d,\"total\":%d,\"remain\":%d,",
        s.mqtt_connected ? "true" : "false", s.gcode_state, name,
        s.progress_pct, s.layer_num, s.total_layers, s.remaining_min);
    p += snprintf(o + p, n - p,
        "\"nozzle\":%.0f,\"nozzle_t\":%.0f,\"bed\":%.0f,\"bed_t\":%.0f,\"chamber\":%.0f,",
        s.nozzle_temp, s.nozzle_target, s.bed_temp, s.bed_target, s.chamber_temp);
    p += snprintf(o + p, n - p, "\"light\":%s,\"fan\":%d,\"speed\":%d,\"active_tray\":%d,\"short\":%.0f,\"prints\":%d,\"used\":%.0f,\"cost\":%.2f,",
        s.light_on ? "true" : "false", s.fan_speed_pct, s.speed_level, s.active_tray_now,
        filament_shortfall(), g_stat_prints, g_stat_grams, g_hist_total_cost);
    p += snprintf(o + p, n - p,
        "\"cfg\":{\"ip\":\"%s\",\"serial\":\"%s\",\"view3d\":%s,\"bri\":%d,\"code_set\":%s,\"scale_ip\":\"%s\",\"low\":%d,\"ntfy\":\"%s\"},",
        g_printer_ip, g_printer_serial, g_screensaver_3d ? "true" : "false",
        g_brightness, g_printer_access_code[0] ? "true" : "false", g_scale_ip, g_low_threshold_g, g_ntfy_topic);

    p += snprintf(o + p, n - p, "\"ams\":[");
    bool first = true;
    for (int u = 0; u < s.ams_count && u < AMS_MAX_UNITS; u++) {
        AmsUnit& un = s.ams[u];
        if (!un.present) continue;
        p += snprintf(o + p, n - p, "%s{\"id\":%d,\"humidity\":%d,\"temp\":%.0f,\"trays\":[",
            first ? "" : ",", u + 1, un.humidity, un.temp);
        first = false;
        for (int t = 0; t < AMS_MAX_TRAYS; t++) {
            AmsTraySlot& tr = un.trays[t];
            p += snprintf(o + p, n - p, "%s{\"present\":%s,\"type\":\"%s\",\"rgb\":\"#%06X\",\"remain\":%d,\"gram\":%.0f}",
                t ? "," : "", tr.present ? "true" : "false", tr.present ? tr.type : "",
                (unsigned)(tr.color & 0xFFFFFF), tr.remain, filament_remaining(u * AMS_MAX_TRAYS + t));
        }
        p += snprintf(o + p, n - p, "]}");
    }
    p += snprintf(o + p, n - p, "],");

    AmsTraySlot& e = s.external_spool;
    snprintf(o + p, n - p, "\"ext\":{\"present\":%s,\"type\":\"%s\",\"rgb\":\"#%06X\",\"remain\":%d,\"gram\":%.0f}}",
        e.present ? "true" : "false", e.present ? e.type : "",
        (unsigned)(e.color & 0xFFFFFF), e.remain, filament_remaining(254));
}

static void build_files(const char* path, char* o, int n) {
    FtpEntry entries[FTP_MAX_ENTRIES];
    int count = 0;
    String err;
    const char* usePath = path[0] ? path : "/";
    SDL_LockMutex(g_ftpmtx);
    bool ok = bambu_ftp_list(usePath, entries, FTP_MAX_ENTRIES, &count, &err);
    SDL_UnlockMutex(g_ftpmtx);

    int p = 0;
    char pe[160];
    json_escape(usePath, pe, sizeof(pe));
    if (!ok) {
        char ee[160];
        json_escape(err.c_str() ? err.c_str() : "fout", ee, sizeof(ee));
        snprintf(o, n, "{\"ok\":false,\"path\":\"%s\",\"err\":\"%s\",\"items\":[]}", pe, ee);
        return;
    }
    p += snprintf(o + p, n - p, "{\"ok\":true,\"path\":\"%s\",\"items\":[", pe);
    for (int i = 0; i < count; i++) {
        char nm[160];
        json_escape(entries[i].name.c_str(), nm, sizeof(nm));
        p += snprintf(o + p, n - p, "%s{\"name\":\"%s\",\"dir\":%s,\"size\":%u}",
            i ? "," : "", nm, entries[i].is_dir ? "true" : "false", (unsigned)entries[i].size);
    }
    snprintf(o + p, n - p, "]}");
}

static void build_spools(char* o, int n) {
    int p = 0;
    p += snprintf(o + p, n - p, "[");
    for (int i = 0; i < g_spool_count; i++) {
        Spool& s = g_spools[i];
        char nm[80], mat[24], code[24], note[160];
        json_escape(s.name, nm, sizeof(nm));
        json_escape(s.material, mat, sizeof(mat));
        json_escape(s.code, code, sizeof(code));
        json_escape(s.note, note, sizeof(note));
        p += snprintf(o + p, n - p,
            "%s{\"i\":%d,\"name\":\"%s\",\"material\":\"%s\",\"rgb\":\"#%06X\",\"rem\":%.0f,\"empty\":%.0f,\"nmin\":%d,\"nmax\":%d,\"code\":\"%s\",\"note\":\"%s\",\"price\":%.2f}",
            i ? "," : "", i, nm, mat, (unsigned)(s.color & 0xFFFFFF),
            s.remaining_g, s.empty_g, s.nmin, s.nmax, code, note, s.price_kg);
    }
    snprintf(o + p, n - p, "]");
}

static void build_empties(char* o, int n) {
    int p = 0;
    p += snprintf(o + p, n - p, "[");
    for (int i = 0; i < g_empty_count; i++) {
        char nm[80];
        json_escape(g_empties[i].name, nm, sizeof(nm));
        p += snprintf(o + p, n - p, "%s{\"i\":%d,\"name\":\"%s\",\"weight\":%.0f}",
                      i ? "," : "", i, nm, g_empties[i].weight_g);
    }
    snprintf(o + p, n - p, "]");
}

static void build_history(char* o, int n) {
    int p = 0;
    p += snprintf(o + p, n - p, "[");
    for (int i = 0; i < g_hist_count; i++) {
        PrintRec& r = g_history[i];
        char nm[96];
        json_escape(r.name, nm, sizeof(nm));
        p += snprintf(o + p, n - p, "%s{\"when\":\"%s\",\"name\":\"%s\",\"grams\":%.0f,\"cost\":%.2f,\"ok\":%d}",
            i ? "," : "", r.when, nm, r.grams, r.cost, r.ok);
    }
    snprintf(o + p, n - p, "]");
}

// --- the single-page control app ----------------------------------------
static const char PAGE_HTML[] =
#include "webctl_page.h"
;

// --- request routing -----------------------------------------------------
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
        return;
    }
    if (!strcmp(path, "/status")) {
        char* js = (char*)malloc(3000);
        if (js) { build_status(js, 3000); send_resp(fd, "200 OK", "application/json", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/files")) {
        char pth[200];
        parse_query(query, "path", pth, sizeof(pth));
        char* js = (char*)malloc(8192);
        if (js) { build_files(pth, js, 8192); send_resp(fd, "200 OK", "application/json", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/cmd")) {
        char a[16], sbuf[16];
        parse_query(query, "a", a, sizeof(a));
        parse_query(query, "s", sbuf, sizeof(sbuf));
        float step = sbuf[0] ? (float)atof(sbuf) : 1.0f;
        int code = code_from_name(a);
        if (!code) { send_resp(fd, "400 Bad Request", "text/plain; charset=utf-8", "onbekend commando", 17); return; }
        const char* reason = "";
        if (move_blocked(code, &reason)) { send_resp(fd, "200 OK", "text/plain; charset=utf-8", reason, (int)strlen(reason)); return; }
        q_push(Q_MOVE, code, step, nullptr);
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "verstuurd", 9);
        return;
    }
    if (!strcmp(path, "/ctl")) {
        char a[16], vbuf[16];
        parse_query(query, "a", a, sizeof(a));
        parse_query(query, "v", vbuf, sizeof(vbuf));
        float v = vbuf[0] ? (float)atof(vbuf) : 0.0f;
        int code = 0;
        if (!strcmp(a, "pause")) code = CTL_PAUSE;
        else if (!strcmp(a, "resume")) code = CTL_RESUME;
        else if (!strcmp(a, "stop")) code = CTL_STOP;
        else if (!strcmp(a, "light")) code = CTL_LIGHT;
        else if (!strcmp(a, "fan")) code = CTL_FAN;
        else if (!strcmp(a, "speed")) code = CTL_SPEED;
        if (!code) { send_resp(fd, "400 Bad Request", "text/plain", "", 0); return; }
        q_push(Q_CTL, code, v, nullptr);
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "verstuurd", 9);
        return;
    }
    if (!strcmp(path, "/start")) {
        char pth[200];
        parse_query(query, "path", pth, sizeof(pth));
        if (!pth[0]) { send_resp(fd, "400 Bad Request", "text/plain", "", 0); return; }
        q_push(Q_START, 0, 0, pth);
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "print-start verstuurd", 21);
        return;
    }
    if (!strcmp(path, "/setcfg")) {
        q_push(Q_CFG, 0, 0, query ? query : "");
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "opgeslagen", 10);
        return;
    }
    if (!strcmp(path, "/spools")) {
        char* js = (char*)malloc(12000);
        if (js) { build_spools(js, 12000); send_resp(fd, "200 OK", "application/json", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/spool_save")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_SPOOL_SAVE, idxs[0] ? atoi(idxs) : -1, 0, query ? query : "");
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "opgeslagen", 10);
        return;
    }
    if (!strcmp(path, "/spool_del")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_SPOOL_DEL, atoi(idxs), 0, nullptr);
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "verwijderd", 10);
        return;
    }
    if (!strcmp(path, "/spool_load")) {
        char idxs[8], slots[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        parse_query(query, "slot", slots, sizeof(slots));
        q_push(Q_SPOOL_LOAD, atoi(idxs), (float)atoi(slots), nullptr);
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "geladen", 7);
        return;
    }
    if (!strcmp(path, "/empties")) {
        char* js = (char*)malloc(4000);
        if (js) { build_empties(js, 4000); send_resp(fd, "200 OK", "application/json", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/empty_save")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_EMPTY_SAVE, idxs[0] ? atoi(idxs) : -1, 0, query ? query : "");
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "opgeslagen", 10);
        return;
    }
    if (!strcmp(path, "/empty_del")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_EMPTY_DEL, atoi(idxs), 0, nullptr);
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "verwijderd", 10);
        return;
    }
    if (!strcmp(path, "/spool_bulk")) {
        q_push(Q_SPOOL_BULK, 0, 0, query ? query : "");
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "ok", 2);
        return;
    }
    if (!strcmp(path, "/notify_test")) {
        notify_send("PandaTouch", "Test melding - het werkt!");
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "testmelding verstuurd", 21);
        return;
    }
    if (!strcmp(path, "/history")) {
        char* js = (char*)malloc(8192);
        if (js) { build_history(js, 8192); send_resp(fd, "200 OK", "application/json", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    send_resp(fd, "404 Not Found", "text/plain", "", 0);
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
    listen(ls, 8);
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
    if (g_qmtx) return;
    g_qmtx = SDL_CreateMutex();
    g_ftpmtx = SDL_CreateMutex();
    detect_url();
    SDL_Thread* th = SDL_CreateThread(server_thread, "webctl", nullptr);
    if (th) SDL_DetachThread(th);
}
