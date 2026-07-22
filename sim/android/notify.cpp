// Push notifications via ntfy.sh. A tiny HTTPS POST to https://ntfy.sh/<topic>
// on a background thread (mbedTLS via the WiFiClientSecure shim, setInsecure -
// the payload is only a print-status line). notify_loop() watches for print
// finish/fail and filament shortfall and pushes a message.
#include "notify.h"
#include "bambu_mqtt.h"
#include "filament_track.h"
#include "backup.h"
#include "spool_db.h"
#include "storage.h"      // g_dry_* reminder state + save_settings()
#include <ctime>
#include <SDL.h>
#include <Arduino.h>
#include "WiFiClientSecure.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

struct NMsg { char title[48]; char body[176]; };

static int nthread(void* data) {
    NMsg* m = (NMsg*)data;
    if (g_ntfy_topic[0]) {
        WiFiClientSecure c;
        c.setInsecure();
        if (c.connect("ntfy.sh", 443)) {
            size_t blen = strlen(m->body);
            char hdr[384];
            int n = snprintf(hdr, sizeof(hdr),
                "POST /%s HTTP/1.1\r\nHost: ntfy.sh\r\nTitle: %s\r\n"
                "Content-Type: text/plain\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
                g_ntfy_topic, m->title, (unsigned)blen);
            c.write((const uint8_t*)hdr, (size_t)n);
            c.write((const uint8_t*)m->body, blen);
            uint32_t t0 = SDL_GetTicks();       // brief drain so the POST completes
            while (SDL_GetTicks() - t0 < 1500) {
                if (c.available() > 0) c.read();
                else SDL_Delay(20);
                if (!c.connected()) break;
            }
            c.stop();
            Serial.printf("NTFY: sent '%s'\n", m->title);
        } else {
            Serial.println("NTFY: connect failed");
        }
    }
    free(m);
    return 0;
}

void notify_send(const char* title, const char* msg) {
    if (!g_ntfy_topic[0]) return;
    NMsg* m = (NMsg*)malloc(sizeof(NMsg));
    if (!m) return;
    strncpy(m->title, title, sizeof(m->title) - 1); m->title[sizeof(m->title) - 1] = 0;
    strncpy(m->body, msg, sizeof(m->body) - 1);    m->body[sizeof(m->body) - 1] = 0;
    SDL_Thread* th = SDL_CreateThread(nthread, "ntfy", m);
    if (th) SDL_DetachThread(th); else free(m);
}

#ifdef __ANDROID__
// Like notify_send(), but attaches the printed model's thumbnail (extracted
// from its .3mf over FTP) so the "print done" push shows a picture. Falls back
// to a plain text push when there's no thumbnail. Runs on its own thread since
// the .3mf download can take a while.
#include "thumb.h"
#include "gcode_view.h"   // active plate of a multi-plate print
struct NThumb { char title[48]; char body[176]; char path[160]; int plate; };

static int nthumb_thread(void* data) {
    NThumb* m = (NThumb*)data;
    uint32_t plen = 0;
    uint8_t* png = m->path[0] ? threemf_thumb(m->path, &plen, m->plate) : nullptr;
    if (g_ntfy_topic[0]) {
        WiFiClientSecure c;
        c.setInsecure();
        if (c.connect("ntfy.sh", 443)) {
            char hdr[512];
            if (png && plen) {
                int n = snprintf(hdr, sizeof(hdr),
                    "POST /%s HTTP/1.1\r\nHost: ntfy.sh\r\nTitle: %s\r\nMessage: %s\r\n"
                    "Filename: print.png\r\nContent-Type: image/png\r\nContent-Length: %u\r\n"
                    "Connection: close\r\n\r\n",
                    g_ntfy_topic, m->title, m->body, (unsigned)plen);
                c.write((const uint8_t*)hdr, (size_t)n);
                c.write(png, plen);
            } else {
                size_t blen = strlen(m->body);
                int n = snprintf(hdr, sizeof(hdr),
                    "POST /%s HTTP/1.1\r\nHost: ntfy.sh\r\nTitle: %s\r\n"
                    "Content-Type: text/plain\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",
                    g_ntfy_topic, m->title, (unsigned)blen);
                c.write((const uint8_t*)hdr, (size_t)n);
                c.write((const uint8_t*)m->body, blen);
            }
            uint32_t t0 = SDL_GetTicks();
            while (SDL_GetTicks() - t0 < 3000) {
                if (c.available() > 0) c.read();
                else SDL_Delay(20);
                if (!c.connected()) break;
            }
            c.stop();
            Serial.printf("NTFY: sent '%s'%s\n", m->title, (png && plen) ? " +image" : "");
        } else {
            Serial.println("NTFY: connect failed");
        }
    }
    if (png) free(png);
    free(m);
    return 0;
}

static void notify_send_thumb(const char* title, const char* msg, const char* path) {
    if (!g_ntfy_topic[0]) return;
    NThumb* m = (NThumb*)malloc(sizeof(NThumb));
    if (!m) return;
    strncpy(m->title, title, sizeof(m->title) - 1); m->title[sizeof(m->title) - 1] = 0;
    strncpy(m->body, msg, sizeof(m->body) - 1);     m->body[sizeof(m->body) - 1] = 0;
    strncpy(m->path, path ? path : "", sizeof(m->path) - 1); m->path[sizeof(m->path) - 1] = 0;
    m->plate = gcode_view_active_plate();   // the plate this print used
    SDL_Thread* th = SDL_CreateThread(nthumb_thread, "ntfyimg", m);
    if (th) SDL_DetachThread(th); else free(m);
}
#endif

// Persisted "already warned this slot is low" state, so a low roll pings once
// and then stays quiet across app restarts (re-arms when refilled/cleared).
#define LOWNOTIFY_PATH "/sdcard/printward_lownotify.conf"
static bool g_low_notified[AMS_MAX_UNITS * AMS_MAX_TRAYS + 1] = {false};
static bool g_low_loaded = false;

static void lownotify_load() {
    FILE* f = fopen(LOWNOTIFY_PATH, "r");
    if (!f) return;
    char line[160];
    if (fgets(line, sizeof(line), f)) {
        for (char* p = line; *p >= '0' && *p <= '9'; ) {
            int k = atoi(p);
            if (k >= 0 && k <= AMS_MAX_UNITS * AMS_MAX_TRAYS) g_low_notified[k] = true;
            char* c = strchr(p, ','); if (!c) break; p = c + 1;
        }
    }
    fclose(f);
}
static void lownotify_save() {
    FILE* f = fopen(LOWNOTIFY_PATH, "w");
    if (!f) return;
    bool first = true;
    for (int k = 0; k <= AMS_MAX_UNITS * AMS_MAX_TRAYS; k++)
        if (g_low_notified[k]) { fprintf(f, "%s%d", first ? "" : ",", k); first = false; }
    fprintf(f, "\n");
    fclose(f);
}

void notify_loop() {
    static char last_state[16] = "";
    static bool warned_short = false;
    static char last_file[64] = "";
    PrinterStatus& s = g_printer_status;

    bool printing = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
    if (printing && s.gcode_file[0]) { strncpy(last_file, s.gcode_file, sizeof(last_file) - 1); last_file[sizeof(last_file) - 1] = 0; }

    if (strcmp(last_state, s.gcode_state) != 0) {
        bool wasPrinting = (strcmp(last_state, "RUNNING") == 0 || strcmp(last_state, "PAUSE") == 0);
        if (wasPrinting && strcmp(s.gcode_state, "FINISH") == 0) {
            const char* msg = s.task_name[0] ? s.task_name : "De print is klaar.";
#ifdef __ANDROID__
            char pth[160];
            snprintf(pth, sizeof(pth), "/cache/%s", last_file);
            notify_send_thumb("Print klaar", msg, last_file[0] ? pth : "");
#else
            notify_send("Print klaar", msg);
#endif
        } else if (wasPrinting && strcmp(s.gcode_state, "FAILED") == 0) {
            notify_send("Print mislukt", s.task_name[0] ? s.task_name : "De print is mislukt.");
        }
        strncpy(last_state, s.gcode_state, sizeof(last_state) - 1);
        last_state[sizeof(last_state) - 1] = 0;
    }

    float sh = filament_shortfall();
    if (sh > 0 && !warned_short) {
        char b[96];
        snprintf(b, sizeof(b), "Komt ~%.0f g te kort voor deze print.", sh);
        notify_send("Filament tekort", b);
        warned_short = true;
    }
    if (sh <= 0) warned_short = false;

    // Silica-gel drying reminder: once the interval has passed since it was last
    // dried, push a one-off reminder. Persisted, so it stays quiet until "dried"
    // is pressed (which re-arms it). A 0 last-dried means the clock starts now.
    if (g_dry_interval_days > 0 && !g_dry_notified) {
        if (g_dry_last_dried == 0) { g_dry_last_dried = (long)time(nullptr); save_settings(); }
        // Warn "advance" days before the actual due date, so it's a heads-up.
        long warn = g_dry_last_dried + (long)(g_dry_interval_days - g_dry_advance_days) * 86400L;
        if ((long)time(nullptr) >= warn) {
            notify_send("Silicagel drogen", "Tijd om het droogmiddel weer te drogen.");
            g_dry_notified = true;
            save_settings();
        }
    }

    // Low filament: warn once when a weighed slot is below the low threshold.
    // Persisted so it pings a single time (incl. rolls already low right now) and
    // stays quiet across restarts; it re-arms once the slot rises back above the
    // threshold or is cleared.
    if (!g_low_loaded) { lownotify_load(); g_low_loaded = true; }
    bool low_changed = false;
    for (int k = 0; k <= AMS_MAX_UNITS * AMS_MAX_TRAYS; k++) {
        int slot = (k == AMS_MAX_UNITS * AMS_MAX_TRAYS) ? 254 : k;   // last index = external spool
        bool low = filament_slot_low(slot);   // capacity set AND remaining < threshold
        if (low && !g_low_notified[k]) {
            char label[24];
            if (slot == 254) strcpy(label, "Externe spoel");
            else snprintf(label, sizeof(label), "AMS%d slot %d", slot / AMS_MAX_TRAYS + 1, slot % AMS_MAX_TRAYS + 1);
            char b[128];
            snprintf(b, sizeof(b), "%s: nog %.0f g over (onder %d g).", label, filament_remaining(slot), g_low_threshold_g);
            notify_send("Filament bijna op", b);
            g_low_notified[k] = true; low_changed = true;
        } else if (!low && g_low_notified[k]) {
            g_low_notified[k] = false; low_changed = true;   // re-arm (refilled/cleared)
        }
    }
    if (low_changed) lownotify_save();

    // Backup nudge: the /sdcard snapshot dies with the tablet, so remind when no
    // backup has actually LEFT the device for a fortnight. Once a week at most,
    // and only when there is something worth losing.
    if (g_spool_count > 0) {
        long since = backup_seconds_since_dl();          // -1 = never downloaded
        if (since < 0 || since > 14L * 86400L) {
            long last = backup_last_remind(), now_s = (long)time(nullptr);
            if (last <= 0 || now_s - last > 7L * 86400L) {
                if (since < 0) {
                    notify_send("Maak een back-up", "Je hebt nog nooit een back-up gedownload - je rollen en historie staan alleen op de tablet.");
                } else {
                    char b[160];
                    snprintf(b, sizeof(b), "Laatste back-up was %ld dagen geleden. Open de webpagina en download er een.", since / 86400L);
                    notify_send("Maak een back-up", b);
                }
                backup_mark_remind();
            }
        }
    }
}
