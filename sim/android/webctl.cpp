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
#include "ui_printer.h"    // create_printer_ui(): rebuild the screen on a language change
#include "scale_client.h"     // g_scale_ip (exposed to the web Scale tab)
#include "filament_track.h"   // filament_remaining(), g_low_threshold_g
#include "spool_db.h"         // spool library + load-to-AMS
#include "notify.h"           // g_ntfy_topic
#include "stats.h"            // g_stat_prints, g_stat_grams
#include "history.h"          // recent-print log + total cost
#include "ui_screensaver.h"   // g_screensaver_3d, screensaver_view_changed()
#include "thumb.h"            // .3mf model-preview extraction (Files/Historie)
#include "gcode_view.h"       // active plate of a multi-plate print
#include "backup.h"           // download / restore all tablet data
#include "lang.h"             // UI translations, served to the web page via /lang
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
#include <ctime>
#include <sys/stat.h>        // data-file health for /diag
#include <zlib.h>            // inflate .3mf thumbnail PNGs for the Files previews
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <android/log.h>

#define WEBCTL_PORT 8080   // default + fallback; the live port is g_webui_port
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PRINTWARD", __VA_ARGS__)

extern bool g_screensaver_3d;

// --- thread-safe request queue (web thread -> main thread) ---------------
enum QKind { Q_MOVE = 1, Q_CTL, Q_CFG, Q_START, Q_SPOOL_SAVE, Q_SPOOL_DEL, Q_SPOOL_LOAD,
             Q_EMPTY_SAVE, Q_EMPTY_DEL, Q_SPOOL_BULK, Q_SPOOL_CLEAR, Q_HIST_BULK, Q_RESTORE,
             Q_REPORT_WEIGHT, Q_DRYSET, Q_DRYDONE };
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

// Queued work is FIFO, so counting pushes and completions is enough to tell
// whether a specific request has been carried out yet - see q_wait().
static unsigned g_qseq = 0, g_qdone = 0;

// Returns a ticket for q_wait(), or 0 if the queue was full (nothing queued).
static unsigned q_push(int kind, int code, float step, const char* arg) {
    if (!g_qmtx) return 0;
    unsigned seq = 0;
    SDL_LockMutex(g_qmtx);
    int nt = (g_qt + 1) % QCAP;
    if (nt != g_qh) {
        g_q[g_qt].kind = kind;
        g_q[g_qt].code = code;
        g_q[g_qt].step = step;
        if (arg) { strncpy(g_q[g_qt].arg, arg, sizeof(g_q[g_qt].arg) - 1); g_q[g_qt].arg[sizeof(g_q[g_qt].arg) - 1] = 0; }
        else g_q[g_qt].arg[0] = 0;
        g_qt = nt;
        seq = ++g_qseq;
    }
    SDL_UnlockMutex(g_qmtx);
    return seq;
}

// Block this HTTP thread until the main thread has actually run the queued item.
// Normally one frame (~35 ms); the timeout only matters if the UI thread is busy,
// and then answering late beats answering wrongly.
static bool q_wait(unsigned seq, int timeout_ms) {
    if (!seq || !g_qmtx) return false;
    for (int waited = 0; waited <= timeout_ms; waited += 5) {
        SDL_LockMutex(g_qmtx);
        unsigned done = g_qdone;
        SDL_UnlockMutex(g_qmtx);
        if (done >= seq) return true;
        SDL_Delay(5);
    }
    return false;
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
    if (!strcmp(a, "bedheat")) return MOVE_BED_HEAT;
    if (!strcmp(a, "bedcool")) return MOVE_BED_COOL;
    if (!strcmp(a, "setnoz")) return MOVE_SET_NOZZLE;
    if (!strcmp(a, "setbed")) return MOVE_SET_BED;
    if (!strcmp(a, "pla")) return MOVE_PLA;
    if (!strcmp(a, "petg")) return MOVE_PETG;
    if (!strcmp(a, "abs")) return MOVE_ABS;
    if (!strcmp(a, "tpu")) return MOVE_TPU;
    if (!strcmp(a, "fan")) return MOVE_FAN;
    if (!strcmp(a, "motoff")) return MOVE_MOTORS_OFF;
    if (!strcmp(a, "homex")) return MOVE_HOME_X;
    if (!strcmp(a, "homey")) return MOVE_HOME_Y;
    if (!strcmp(a, "homez")) return MOVE_HOME_Z;
    if (!strcmp(a, "center")) return MOVE_CENTER;
    if (!strcmp(a, "front")) return MOVE_FRONT;
    if (!strcmp(a, "zup")) return MOVE_ZUP;
    return 0;
}

// --- who is allowed to talk to us -----------------------------------------

// True for the address ranges a home LAN actually uses: 10/8, 172.16/12,
// 192.168/16, plus loopback and 169.254/16 (link-local, when DHCP failed).
// Deliberately not "anything that isn't public" - unknown means no.
static bool addr_is_local(const struct sockaddr_in* a) {
    uint32_t ip = ntohl(a->sin_addr.s_addr);
    uint8_t b1 = (uint8_t)(ip >> 24), b2 = (uint8_t)(ip >> 16);
    if (b1 == 127) return true;                          // 127.0.0.0/8
    if (b1 == 10) return true;                           // 10.0.0.0/8
    if (b1 == 192 && b2 == 168) return true;             // 192.168.0.0/16
    if (b1 == 172 && b2 >= 16 && b2 <= 31) return true;  // 172.16.0.0/12
    if (b1 == 169 && b2 == 254) return true;             // 169.254.0.0/16
    return false;
}

// base64, for building the Basic-auth string we expect. Encoding what we want and
// comparing strings means we never have to decode attacker-supplied input.
static void b64(const char* in, char* out, int n) {
    static const char* T64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int len = (int)strlen(in), j = 0;
    for (int i = 0; i < len && j < n - 5; i += 3) {
        unsigned v = (unsigned char)in[i] << 16;
        if (i + 1 < len) v |= (unsigned char)in[i + 1] << 8;
        if (i + 2 < len) v |= (unsigned char)in[i + 2];
        out[j++] = T64[(v >> 18) & 63];
        out[j++] = T64[(v >> 12) & 63];
        out[j++] = (i + 1 < len) ? T64[(v >> 6) & 63] : '=';
        out[j++] = (i + 2 < len) ? T64[v & 63] : '=';
    }
    out[j] = 0;
}

// Basic auth over plain HTTP: the password crosses your LAN base64-encoded, which
// is encoding, not encryption. That is a deliberate trade - real TLS on this stack
// means a self-signed cert and a browser warning every visit. It stops the printer
// being open to anyone who finds the port; it does not stop someone sniffing your
// own Wi-Fi. The local-network check above is what makes that trade acceptable.
static bool authorized(const char* buf) {
    if (!g_webui_pass[0]) return true;         // no password set -> no gate (see webui_pass_ensure)
    char want[80], expect[128];
    snprintf(want, sizeof(want), "printward:%s", g_webui_pass);
    char enc[128];
    b64(want, enc, sizeof(enc));
    snprintf(expect, sizeof(expect), "Authorization: Basic %s", enc);
    // Header names are case-insensitive; browsers all send it exactly like this.
    return strstr(buf, expect) != nullptr;
}

// Reply with a one-line message in the UI language. The length has to come from
// the translation itself - the old hand-counted literals only matched the Dutch.
static void send_resp(int fd, const char* status, const char* ctype, const char* body, int blen);
static void send_msg(int fd, const char* status, const char* key) {
    const char* m = T(key);
    send_resp(fd, status, "text/plain; charset=utf-8", m, (int)strlen(m));
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
                if (parse_query(c.arg, "low", v, sizeof(v)) && v[0]) {
                    int lw = atoi(v); if (lw < 0) lw = 0; if (lw > 5000) lw = 5000;
                    if (lw != g_low_threshold_g) { g_low_threshold_g = lw; filament_save(); }
                }
                if (parse_query(c.arg, "lang", v, sizeof(v)) && v[0]) {
                    if (strcmp(v, g_lang)) {
                        lang_set(v);   // persists + reloads the table
                        // A label takes its text when the screen is built, so the
                        // running UI would sit in the old language until a restart -
                        // switching did nothing you could see. Rebuild it here, on the
                        // main thread, where touching LVGL is safe. Sub-screens pick
                        // the new language up when they are next opened.
                        if (g_main_screen) create_printer_ui();
                    }
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
                else if (c.code >= 0 && c.code < g_spool_count) s.price_kg = g_spools[c.code].price_kg;  // don't drop the price when a save (e.g. weigh) omits it
                if (parse_query(c.arg, "num", v, sizeof(v))) s.number = atoi(v);
                else if (c.code >= 0 && c.code < g_spool_count) s.number = g_spools[c.code].number;  // keep the roll number on a partial save
                spool_upsert(c.code, s);          // c.code = idx (-1 = add)
                break;
            }
            case Q_SPOOL_DEL:  spool_delete(c.code); break;
            case Q_SPOOL_LOAD: spool_load_to_slot(c.code, (int)c.step); break;
            case Q_SPOOL_CLEAR: spool_clear_slot((int)c.step); break;
            case Q_DRYSET: {
                g_dry_interval_days = c.code < 0 ? 0 : c.code;
                int adv = (int)c.step; if (adv < 0) adv = 0;
                if (adv > g_dry_interval_days) adv = g_dry_interval_days;   // can't warn before it was dried
                g_dry_advance_days = adv;
                g_dry_banner_always = (c.arg[0] == '1');   // banner always on the tablet vs only within the window
                if (g_dry_interval_days > 0 && g_dry_last_dried == 0) g_dry_last_dried = (long)time(nullptr);
                g_dry_notified = false;               // re-evaluate against the new interval
                save_settings();
                break;
            }
            case Q_DRYDONE:
                g_dry_last_dried = (long)time(nullptr);
                g_dry_notified = false;               // re-arm the reminder
                save_settings();
                break;
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
                } else if (!strcmp(act, "price")) {
                    float pr = (float)atof(v);
                    for (int a = 0; a < n; a++) if (ids[a] >= 0 && ids[a] < g_spool_count) g_spools[ids[a]].price_kg = pr;
                    spool_db_save();
                }
                break;
            }
            case Q_HIST_BULK: {
                char act[16], idxs[300];
                parse_query(c.arg, "act", act, sizeof(act));
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
                    for (int a = 0; a < n; a++) history_remove(ids[a]);
                } else {
                    int v = !strcmp(act, "arch") ? 1 : 0;   // arch -> 1, unarch -> 0
                    for (int a = 0; a < n; a++) history_set_arch(ids[a], v);
                }
                break;
            }
            case Q_RESTORE: {
                // The web thread wrote the uploaded backup to a temp file; apply it
                // here (main thread) and reload every module from the fresh files.
                FILE* f = fopen("/sdcard/printward_restore.tmp", "rb");
                if (f) {
                    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
                    if (n < 0) n = 0;
                    char* d = (char*)malloc(n + 1);
                    if (d) {
                        long got = (long)fread(d, 1, n, f);
                        if (got < 0) got = 0;
                        d[got] = 0;
                        int nrest = backup_apply(d);
                        free(d);
                        spool_db_load(); empty_db_load();
                        filament_track_init(); history_init(); stats_init();
                        Serial.printf("RESTORE: %d files restored\n", nrest);
                    }
                    fclose(f);
                }
                remove("/sdcard/printward_restore.tmp");
                break;
            }
            case Q_REPORT_WEIGHT:
                // Cloud relay reported a finished print's actual grams (in c.step).
                // Correct the live estimate on the right slot (main thread here).
                filament_reconcile_actual(c.step);
                break;
        }
        // One spot, after every case, so q_wait() can see this item is really done.
        SDL_LockMutex(g_qmtx);
        g_qdone++;
        SDL_UnlockMutex(g_qmtx);
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
    float lg = 0, lc = 0;
    bool live = filament_live_cost(&lg, &lc);
    char gf[160]; json_escape(s.gcode_file, gf, sizeof(gf));
    p += snprintf(o + p, n - p, "\"light\":%s,\"fan\":%d,\"speed\":%d,\"active_tray\":%d,\"short\":%.0f,\"prints\":%d,\"used\":%.0f,\"cost\":%.2f,\"printg\":%.0f,\"printcost\":%.2f,\"file\":\"%s\",",
        s.light_on ? "true" : "false", s.fan_speed_pct, s.speed_level, s.active_tray_now,
        filament_shortfall(), g_stat_prints, g_stat_grams, g_hist_total_cost,
        live ? lg : -1.0f, live ? lc : 0.0f, gf);
    p += snprintf(o + p, n - p, "\"plate\":%d,\"plates\":%d,\"mplate\":%d,",
        gcode_view_active_plate(), gcode_view_plate_count(), gcode_view_manual_plate());
    p += snprintf(o + p, n - p, "\"dry\":{\"iv\":%d,\"adv\":%d,\"always\":%d,\"last\":%ld,\"now\":%ld},",
        g_dry_interval_days, g_dry_advance_days, g_dry_banner_always ? 1 : 0, g_dry_last_dried, (long)time(nullptr));
    p += snprintf(o + p, n - p,
        "\"bkage\":%ld,",
        backup_seconds_since_dl());
    p += snprintf(o + p, n - p,
        "\"cfg\":{\"ip\":\"%s\",\"serial\":\"%s\",\"view3d\":%s,\"bri\":%d,\"code_set\":%s,\"scale_ip\":\"%s\",\"low\":%d,\"ntfy\":\"%s\",\"lang\":\"%s\"},",
        g_printer_ip, g_printer_serial, g_screensaver_3d ? "true" : "false",
        g_brightness, g_printer_access_code[0] ? "true" : "false", g_scale_ip, g_low_threshold_g, g_ntfy_topic, lang_current());

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

// Health snapshot: is the printer link alive, is the config complete, and is the
// data actually on disk? Saves digging through adb logcat when something is off.
static void build_diag(char* o, int n) {
    PrinterStatus& s = g_printer_status;
    unsigned long now = millis();
    int p = 0;
    p += snprintf(o + p, n - p,
        "{\"uptime\":%lu,\"mqtt\":%s,\"have_data\":%s,\"age\":%ld,"
        "\"ip\":\"%s\",\"serial_set\":%s,\"code_set\":%s,\"scale_ip\":\"%s\",\"url\":\"%s\","
        "\"bkage\":%ld,\"sd\":\"%s\",\"files\":[",
        now / 1000UL, s.mqtt_connected ? "true" : "false", s.have_data ? "true" : "false",
        s.last_update_ms ? (long)((now - s.last_update_ms) / 1000UL) : -1L,
        g_printer_ip, g_printer_serial[0] ? "true" : "false",
        g_printer_access_code[0] ? "true" : "false", g_scale_ip, webctl_url(),
        backup_seconds_since_dl(), backup_sd_dir());

    struct DiagFile { const char* tag; const char* path; };
    static const DiagFile F[] = {
        {"rollen",     "/sdcard/printward_spools.conf"},
        {"lege spoel", "/sdcard/printward_empties.conf"},
        {"gewichten",  "/sdcard/printward_weights.conf"},
        {"historie",   "/sdcard/printward_history.conf"},
        {"statistiek", "/sdcard/printward_stats.conf"},
        {"snapshot",   "/sdcard/printward_backup/spools.conf"},
    };
    time_t tnow = time(nullptr);
    for (int i = 0; i < (int)(sizeof(F) / sizeof(F[0])); i++) {
        struct stat st;
        bool ok = (stat(F[i].path, &st) == 0);
        p += snprintf(o + p, n - p, "%s{\"n\":\"%s\",\"ok\":%s,\"bytes\":%ld,\"age\":%ld}",
            i ? "," : "", F[i].tag, ok ? "true" : "false",
            ok ? (long)st.st_size : 0L, ok ? (long)(tnow - st.st_mtime) : -1L);
    }
    snprintf(o + p, n - p, "]}");
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
            "%s{\"i\":%d,\"num\":%d,\"name\":\"%s\",\"material\":\"%s\",\"rgb\":\"#%06X\",\"rem\":%.0f,\"live\":%.0f,\"slot\":%d,\"empty\":%.0f,\"nmin\":%d,\"nmax\":%d,\"code\":\"%s\",\"note\":\"%s\",\"price\":%.2f}",
            i ? "," : "", i, s.number, nm, mat, (unsigned)(s.color & 0xFFFFFF),
            s.remaining_g, spool_live_grams(s), s.slot, s.empty_g, s.nmin, s.nmax, code, note, s.price_kg);
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
        char nm[96], fl[96], mt[24];
        json_escape(r.name, nm, sizeof(nm));
        json_escape(r.file, fl, sizeof(fl));
        json_escape(r.mat, mt, sizeof(mt));
        p += snprintf(o + p, n - p, "%s{\"i\":%d,\"when\":\"%s\",\"name\":\"%s\",\"grams\":%.0f,\"cost\":%.2f,\"ok\":%d,\"file\":\"%s\",\"arch\":%d,\"mat\":\"%s\",\"ts\":%ld,\"mins\":%d}",
            i ? "," : "", i, r.when, nm, r.grams, r.cost, r.ok, fl, r.arch, mt, r.ts, r.mins);
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

    // Gate everything, not just the write endpoints: /status alone tells you what
    // is printing and what it cost, and /backup hands over the whole roll library.
    if (!authorized(buf)) {
        const char* body = "Printward: password required. It is on the tablet, under Settings.\n";
        char hdr[256];
        int n = snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Printward\"\r\n"
            "Content-Type: text/plain; charset=utf-8\r\nContent-Length: %d\r\n"
            "Cache-Control: no-store\r\nConnection: close\r\n\r\n", (int)strlen(body));
        send(fd, hdr, n, 0);
        send(fd, body, (int)strlen(body), 0);
        return;
    }

    bool is_post = (strncmp(buf, "POST ", 5) == 0);
    if (!is_post && strncmp(buf, "GET ", 4) != 0) { send_resp(fd, "405 Method Not Allowed", "text/plain", "", 0); return; }
    char* path = buf + (is_post ? 5 : 4);
    char* sp = strchr(path, ' ');
    if (sp) *sp = 0;
    char* query = strchr(path, '?');
    if (query) { *query = 0; query++; }

    // Restore: the browser POSTs the raw backup blob. Read the full body (it can
    // exceed the initial recv), stash it in a temp file and let the main thread
    // apply + reload (touching storage/UI globals off the web thread is unsafe).
    if (is_post && !strcmp(path, "/restore")) {
        char* bodystart = strstr(buf, "\r\n\r\n");
        char* cl = strstr(buf, "Content-Length:");
        if (!cl) cl = strstr(buf, "content-length:");
        int clen = cl ? atoi(cl + 15) : 0;
        if (!bodystart || clen <= 0 || clen > 1024 * 1024) { send_msg(fd, "400 Bad Request", "invalid"); return; }
        bodystart += 4;
        int have = r - (int)(bodystart - buf);
        if (have > clen) have = clen;
        char* body = (char*)malloc(clen + 1);
        if (!body) { send_resp(fd, "500 Error", "text/plain", "", 0); return; }
        memcpy(body, bodystart, have);
        while (have < clen) {
            int k = (int)recv(fd, body + have, clen - have, 0);
            if (k <= 0) break;
            have += k;
        }
        body[have] = 0;
        FILE* tf = fopen("/sdcard/printward_restore.tmp", "wb");
        if (tf) {
            fwrite(body, 1, have, tf);
            fclose(tf);
            q_push(Q_RESTORE, 0, 0, nullptr);
            send_msg(fd, "200 OK", "bk.restored");
        } else {
            send_msg(fd, "500 Error", "write_failed");
        }
        free(body);
        return;
    }

    if (!strcmp(path, "/")) {
        send_resp(fd, "200 OK", "text/html; charset=utf-8", PAGE_HTML, (int)strlen(PAGE_HTML));
        return;
    }
    if (!strcmp(path, "/backup")) {          // download all tablet data as one file
        int len = 0;
        char* blob = backup_build(&len);
        if (blob && len > 0) {
            char hdr[256];
            int hn = snprintf(hdr, sizeof(hdr),
                "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
                "Content-Disposition: attachment; filename=\"printward-backup.ptb\"\r\n"
                "Content-Length: %d\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n", len);
            send(fd, hdr, hn, 0);
            send(fd, blob, len, 0);
            backup_mark_downloaded();   // a copy just left the device: reset the nudge
        } else {
            send_resp(fd, "500 Error", "text/plain", "", 0);
        }
        free(blob);
        return;
    }
    if (!strcmp(path, "/status")) {
        char* js = (char*)malloc(3000);
        if (js) { build_status(js, 3000); send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/files")) {
        char pth[200];
        parse_query(query, "path", pth, sizeof(pth));
        char* js = (char*)malloc(8192);
        if (js) { build_files(pth, js, 8192); send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/delete")) {          // delete one file over FTP (DELE)
        char pth[220];
        parse_query(query, "path", pth, sizeof(pth));
        if (!pth[0]) { send_msg(fd, "400 Bad Request", "no_path"); return; }
        String err;
        if (bambu_ftp_delete(pth, &err)) send_msg(fd, "200 OK", "deleted");
        else { String m = String(T("error")) + ": " + err; send_resp(fd, "500 Error", "text/plain; charset=utf-8", m.c_str(), (int)m.length()); }
        return;
    }
    if (!strcmp(path, "/thumb")) {           // .3mf model preview (embedded PNG)
        char pth[220], plbuf[8];
        parse_query(query, "path", pth, sizeof(pth));
        parse_query(query, "plate", plbuf, sizeof(plbuf));
        int plate = plbuf[0] ? atoi(plbuf) : 1;
        uint32_t len = 0;
        uint8_t* png = pth[0] ? threemf_thumb(pth, &len, plate) : nullptr;
        if (png && len) { send_resp(fd, "200 OK", "image/png", (const char*)png, (int)len); free(png); }
        else { if (png) free(png); send_resp(fd, "404 Not Found", "text/plain", "", 0); }
        return;
    }
    if (!strcmp(path, "/setplate")) {        // multi-plate: force a plate (0 = auto)
        char plbuf[8];
        parse_query(query, "p", plbuf, sizeof(plbuf));
        gcode_view_set_manual_plate(plbuf[0] ? atoi(plbuf) : 0);
        send_msg(fd, "200 OK", "ok");
        return;
    }
    if (!strcmp(path, "/hist_bulk")) {       // archive/restore/delete history entries
        q_push(Q_HIST_BULK, 0, 0, query ? query : "");
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "ok", 2);
        return;
    }
    if (!strcmp(path, "/cmd")) {
        char a[16], sbuf[16];
        parse_query(query, "a", a, sizeof(a));
        parse_query(query, "s", sbuf, sizeof(sbuf));
        float step = sbuf[0] ? (float)atof(sbuf) : 1.0f;
        int code = code_from_name(a);
        if (!code) { send_msg(fd, "400 Bad Request", "unknown_cmd"); return; }
        const char* reason = "";
        if (move_blocked(code, &reason)) { send_resp(fd, "200 OK", "text/plain; charset=utf-8", reason, (int)strlen(reason)); return; }
        q_push(Q_MOVE, code, step, nullptr);
        send_msg(fd, "200 OK", "sent");
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
        send_msg(fd, "200 OK", "sent");
        return;
    }
    if (!strcmp(path, "/start")) {
        char pth[200];
        parse_query(query, "path", pth, sizeof(pth));
        if (!pth[0]) { send_resp(fd, "400 Bad Request", "text/plain", "", 0); return; }
        // Same gate as the tablet: 1.08+ only accepts a third-party print start in
        // LAN-only mode. Off, the web says so rather than pretending it worked.
        if (!g_lan_mode) { send_msg(fd, "200 OK", "files.print_blocked"); return; }
        q_push(Q_START, 0, 0, pth);
        send_msg(fd, "200 OK", "print_start_sent");
        return;
    }
    if (!strcmp(path, "/setcfg")) {
        // Wait for the main thread to apply this before answering. Without it the
        // page fetched /lang the moment "saved" came back, raced the queue, got the
        // old table and left the UI unchanged - which felt like "the language switch
        // needs several clicks". Costs about one frame.
        q_wait(q_push(Q_CFG, 0, 0, query ? query : ""), 1500);
        send_msg(fd, "200 OK", "saved");
        return;
    }
    if (!strcmp(path, "/setdry")) {          // silica-gel reminder interval (days; 0 = off) + advance days
        char d[8], a[8], al[4];
        parse_query(query, "days", d, sizeof(d));
        parse_query(query, "adv", a, sizeof(a));
        parse_query(query, "always", al, sizeof(al));
        q_push(Q_DRYSET, d[0] ? atoi(d) : 0, a[0] ? (float)atoi(a) : 0.0f, al[0] ? al : "0");
        send_msg(fd, "200 OK", "ok");
        return;
    }
    if (!strcmp(path, "/drydone")) {         // mark the desiccant dried right now
        q_push(Q_DRYDONE, 0, 0, nullptr);
        send_msg(fd, "200 OK", "ok");
        return;
    }
    if (!strcmp(path, "/spools")) {
        char* js = (char*)malloc(12000);
        if (js) { build_spools(js, 12000); send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/spool_save")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_SPOOL_SAVE, idxs[0] ? atoi(idxs) : -1, 0, query ? query : "");
        send_msg(fd, "200 OK", "saved");
        return;
    }
    if (!strcmp(path, "/spool_del")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_SPOOL_DEL, atoi(idxs), 0, nullptr);
        send_msg(fd, "200 OK", "deleted");
        return;
    }
    if (!strcmp(path, "/spool_load")) {
        char idxs[8], slots[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        parse_query(query, "slot", slots, sizeof(slots));
        q_push(Q_SPOOL_LOAD, atoi(idxs), (float)atoi(slots), nullptr);
        send_msg(fd, "200 OK", "loaded");
        return;
    }
    if (!strcmp(path, "/spool_clear")) {
        char slots[8];
        parse_query(query, "slot", slots, sizeof(slots));
        q_push(Q_SPOOL_CLEAR, 0, (float)atoi(slots), nullptr);
        send_msg(fd, "200 OK", "empty");
        return;
    }
    if (!strcmp(path, "/empties")) {
        char* js = (char*)malloc(4000);
        if (js) { build_empties(js, 4000); send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/empty_save")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_EMPTY_SAVE, idxs[0] ? atoi(idxs) : -1, 0, query ? query : "");
        send_msg(fd, "200 OK", "saved");
        return;
    }
    if (!strcmp(path, "/empty_del")) {
        char idxs[8];
        parse_query(query, "idx", idxs, sizeof(idxs));
        q_push(Q_EMPTY_DEL, atoi(idxs), 0, nullptr);
        send_msg(fd, "200 OK", "deleted");
        return;
    }
    if (!strcmp(path, "/spool_bulk")) {
        q_push(Q_SPOOL_BULK, 0, 0, query ? query : "");
        send_resp(fd, "200 OK", "text/plain; charset=utf-8", "ok", 2);
        return;
    }
    if (!strcmp(path, "/notify_test")) {
        notify_send("Printward", "Test melding - het werkt!");
        send_msg(fd, "200 OK", "test_sent");
        return;
    }
    if (!strcmp(path, "/lang")) {          // active translation table for the web page
        char* js = (char*)malloc(16384);
        if (js) { lang_json(js, 16384); send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/langs")) {         // which languages are available (built-in + files)
        char codes[16][8];
        int n = lang_codes(codes, 16);
        char js[256]; int p = 0;
        p += snprintf(js + p, sizeof(js) - p, "[");
        for (int i = 0; i < n; i++) p += snprintf(js + p, sizeof(js) - p, "%s\"%s\"", i ? "," : "", codes[i]);
        snprintf(js + p, sizeof(js) - p, "]");
        send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js));
        return;
    }
    if (!strcmp(path, "/diag")) {
        char* js = (char*)malloc(2048);
        if (js) { build_diag(js, 2048); send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    if (!strcmp(path, "/history")) {
        char* js = (char*)malloc(24576);
        if (js) { build_history(js, 24576); send_resp(fd, "200 OK", "application/json; charset=utf-8", js, (int)strlen(js)); free(js); }
        else send_resp(fd, "500 Error", "text/plain", "", 0);
        return;
    }
    // --- Bambu Cloud weight relay (tools/bambu_weight_relay.py) ----------------
    // The relay runs on a PC, logs into Bambu Cloud (which the tablet can't - it's
    // behind Cloudflare), and reports each finished print's real filament use here.
    // That corrects the live estimate, which matters most for people without the
    // scale, who can't re-weigh to fix the estimate's drift. Params are in the
    // query string (the relay sends them that way); auth is the normal web password.
    if (!strcmp(path, "/api/report_weight")) {
        char w[24], key[80];
        parse_query(query, "weight_g", w, sizeof(w));
        parse_query(query, "task_key", key, sizeof(key));
        float grams = w[0] ? (float)atof(w) : 0;
        // Dedup: the relay reports each print once, but ignore a repeat of the last
        // task_key in case a cycle overlaps. Newer keys sort greater (they're ISO
        // timestamps), matching how the relay walks its list oldest-first.
        static char last_key[80] = "";
        if (grams > 0 && (!key[0] || strcmp(key, last_key) > 0)) {
            if (key[0]) { strncpy(last_key, key, sizeof(last_key) - 1); last_key[sizeof(last_key) - 1] = 0; }
            q_push(Q_REPORT_WEIGHT, 0, grams, nullptr);
        }
        send_resp(fd, "200 OK", "application/json; charset=utf-8", "{\"result\":\"ok\"}", 15);
        return;
    }
    if (!strcmp(path, "/api/cloud_status")) {
        // The tablet no longer polls Bambu Cloud itself (the relay does), so tell
        // the relay we're "logged in" - that stops it trying to push a token here.
        const char* b = "{\"logged_in\":true}";
        send_resp(fd, "200 OK", "application/json; charset=utf-8", b, (int)strlen(b));
        return;
    }
    send_resp(fd, "404 Not Found", "text/plain", "", 0);
}

static int g_bound_port = 0;   // the port we actually got, for webctl_url()
static void detect_url();      // defined below; called once we know the real port

static int server_thread(void*) {
    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { LOGI("WEBCTL: socket() failed"); return 0; }
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    // Try the configured port, then fall back to 8080. A bad or already-taken port
    // must not lock the user out of the only place they can fix it - the web page
    // itself. (Ports below 1024 are filtered on load, since a non-root app can't
    // bind them, but a port that's simply in use can still fail here.)
    int want = g_webui_port, got = 0;
    for (int attempt = 0; attempt < 2 && !got; attempt++) {
        int p = (attempt == 0) ? want : WEBCTL_PORT;
        if (attempt == 1 && p == want) break;         // fallback == configured, no point retrying
        addr.sin_port = htons((uint16_t)p);
        if (bind(ls, (sockaddr*)&addr, sizeof(addr)) == 0) { got = p; break; }
        LOGI("WEBCTL: bind :%d failed%s", p, attempt == 0 ? " - falling back to 8080" : "");
    }
    if (!got) { close(ls); return 0; }
    g_bound_port = got;
    detect_url();                 // rebuild the URL now that we know the real port
    listen(ls, 8);
    LOGI("WEBCTL: control page on %s", webctl_url());
    for (;;) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int fd = accept(ls, (struct sockaddr*)&peer, &plen);
        if (fd < 0) continue;
        // Refuse anything that isn't on the local network. This is the backstop
        // for the failure that actually happens: someone forwards port 8080 on the
        // router, or UPnP does it for them, and the printer is on the internet.
        // A password alone would still leave the whole thing reachable.
        if (!g_allow_remote && !addr_is_local(&peer)) {
            char who[INET_ADDRSTRLEN] = "?";
            inet_ntop(AF_INET, &peer.sin_addr, who, sizeof(who));
            LOGI("WEBCTL: refused %s - not on the local network (allow_remote=0)", who);
            const char* body = "Printward only answers on your own network.\n";
            send_resp(fd, "403 Forbidden", "text/plain; charset=utf-8", body, (int)strlen(body));
            close(fd);
            continue;
        }
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
    int port = g_bound_port ? g_bound_port : g_webui_port;
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
            snprintf(g_url, sizeof(g_url), "http://%s:%d", ip, port);
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
