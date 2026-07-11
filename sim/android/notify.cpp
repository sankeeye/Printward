// Push notifications via ntfy.sh. A tiny HTTPS POST to https://ntfy.sh/<topic>
// on a background thread (mbedTLS via the WiFiClientSecure shim, setInsecure -
// the payload is only a print-status line). notify_loop() watches for print
// finish/fail and filament shortfall and pushes a message.
#include "notify.h"
#include "bambu_mqtt.h"
#include "filament_track.h"
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

void notify_loop() {
    static char last_state[16] = "";
    static bool warned_short = false;
    PrinterStatus& s = g_printer_status;

    if (strcmp(last_state, s.gcode_state) != 0) {
        bool wasPrinting = (strcmp(last_state, "RUNNING") == 0 || strcmp(last_state, "PAUSE") == 0);
        if (wasPrinting && strcmp(s.gcode_state, "FINISH") == 0)
            notify_send("Print klaar", s.task_name[0] ? s.task_name : "De print is klaar.");
        else if (wasPrinting && strcmp(s.gcode_state, "FAILED") == 0)
            notify_send("Print mislukt", s.task_name[0] ? s.task_name : "De print is mislukt.");
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
}
