// Recent print log + total filament cost.
#include "history.h"
#include "bambu_mqtt.h"
#include "gcode_view.h"
#include "filament_track.h"
#include <Arduino.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdlib>

PrintRec g_history[HIST_MAX];
int      g_hist_count = 0;
float    g_hist_total_cost = 0;

#define HIST_PATH "/sdcard/pandatouch_history.conf"

static void sani(char* s) { for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r') *s = ' '; }

static void history_save() {
    FILE* f = fopen(HIST_PATH, "w");
    if (!f) return;
    for (int i = 0; i < g_hist_count; i++) {
        PrintRec r = g_history[i];
        sani(r.when); sani(r.name);
        fprintf(f, "%s|%s|%.1f|%.2f|%d\n", r.when, r.name, r.grams, r.cost, r.ok);
    }
    fclose(f);
}

static char* fld(char** p) {
    char* s = *p;
    char* bar = strchr(s, '|');
    if (bar) { *bar = 0; *p = bar + 1; } else *p = s + strlen(s);
    return s;
}

void history_init() {
    g_hist_count = 0; g_hist_total_cost = 0;
    FILE* f = fopen(HIST_PATH, "r");
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof(line), f) && g_hist_count < HIST_MAX) {
        char* nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        if (!line[0]) continue;
        char* p = line;
        PrintRec r; memset(&r, 0, sizeof(r));
        strncpy(r.when, fld(&p), sizeof(r.when) - 1);
        strncpy(r.name, fld(&p), sizeof(r.name) - 1);
        r.grams = (float)atof(fld(&p));
        r.cost = (float)atof(fld(&p));
        r.ok = atoi(fld(&p));
        g_history[g_hist_count++] = r;
        g_hist_total_cost += r.cost;
    }
    fclose(f);
}

void history_loop() {
    static char last[16] = "";
    static int last_active = 0;
    PrinterStatus& s = g_printer_status;

    bool printing = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
    if (printing && s.active_tray_now >= 0 && s.active_tray_now != 255) last_active = s.active_tray_now;

    if (strcmp(last, s.gcode_state) != 0) {
        bool wasPrinting = (strcmp(last, "RUNNING") == 0 || strcmp(last, "PAUSE") == 0);
        bool fin = (strcmp(s.gcode_state, "FINISH") == 0);
        bool fail = (strcmp(s.gcode_state, "FAILED") == 0);
        if (wasPrinting && (fin || fail)) {
            PrintRec r; memset(&r, 0, sizeof(r));
            time_t now = time(nullptr);
            struct tm* lt = localtime(&now);
            if (lt) strftime(r.when, sizeof(r.when), "%d-%m %H:%M", lt); else strcpy(r.when, "?");
            strncpy(r.name, s.task_name[0] ? s.task_name : "print", sizeof(r.name) - 1);
            r.grams = gcode_view_ready() ? gcode_view_filament_g() : 0.0f;
            r.ok = fin ? 1 : 0;
            r.cost = r.grams * filament_price(last_active) / 1000.0f;
            for (int i = (g_hist_count < HIST_MAX ? g_hist_count : HIST_MAX - 1); i > 0; i--)
                g_history[i] = g_history[i - 1];
            g_history[0] = r;              // newest first
            if (g_hist_count < HIST_MAX) g_hist_count++;
            g_hist_total_cost += r.cost;
            history_save();
            Serial.printf("HISTORY: %s %s %.0fg EUR%.2f\n", r.ok ? "OK" : "FAIL", r.name, r.grams, r.cost);
        }
        strncpy(last, s.gcode_state, sizeof(last) - 1);
        last[sizeof(last) - 1] = 0;
    }
}
