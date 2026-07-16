// Lifetime print statistics: prints finished + total filament grams.
#include "stats.h"
#include "bambu_mqtt.h"
#include "gcode_view.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

int   g_stat_prints = 0;
float g_stat_grams = 0;

#define STATS_PATH "/sdcard/printward_stats.conf"

static void stats_save() {
    FILE* f = fopen(STATS_PATH, "w");
    if (!f) return;
    fprintf(f, "prints=%d\ngrams=%.1f\n", g_stat_prints, g_stat_grams);
    fclose(f);
}

void stats_init() {
    FILE* f = fopen(STATS_PATH, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        char* e = strchr(line, '=');
        if (!e) continue;
        *e = 0;
        if (!strcmp(line, "prints")) g_stat_prints = atoi(e + 1);
        else if (!strcmp(line, "grams")) g_stat_grams = (float)atof(e + 1);
    }
    fclose(f);
}

void stats_loop() {
    static char last[16] = "";
    PrinterStatus& s = g_printer_status;
    if (strcmp(last, s.gcode_state) != 0) {
        bool wasPrinting = (strcmp(last, "RUNNING") == 0 || strcmp(last, "PAUSE") == 0);
        if (wasPrinting && strcmp(s.gcode_state, "FINISH") == 0) {
            float g = gcode_view_ready() ? gcode_view_filament_g() : 0.0f;
            if (g > 0) {
                g_stat_grams += g;
                g_stat_prints++;
                stats_save();
                Serial.printf("STATS: +%.0fg -> %d prints, %.0fg total\n", g, g_stat_prints, g_stat_grams);
            }
        }
        strncpy(last, s.gcode_state, sizeof(last) - 1);
        last[sizeof(last) - 1] = 0;
    }
}
