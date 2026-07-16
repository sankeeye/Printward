// Scale-fed filament tracking. Turns a weighed spool + the running print's
// gcode filament total into a live remaining-grams estimate per AMS slot.
#include "filament_track.h"
#include "bambu_mqtt.h"    // g_printer_status, AMS_MAX_*
#include "gcode_view.h"    // gcode_view_ready(), gcode_view_filament_g()
#include "storage.h"       // g_tray_capacity_g/used_g, g_ext_*
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

int g_low_threshold_g = 100;

#define WEIGHTS_PATH "/sdcard/printward_weights.conf"
#define EXT_SLOT 254

static float g_tray_price[AMS_MAX_UNITS][AMS_MAX_TRAYS] = {{0}};   // EUR/kg per slot
static float g_ext_price = 0;

// --- slot <-> storage mapping --------------------------------------------
static float cap_of(int slot) {
    if (slot == EXT_SLOT) return g_ext_capacity_g;
    int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
    if (u >= 0 && u < AMS_MAX_UNITS && t >= 0 && t < AMS_MAX_TRAYS) return g_tray_capacity_g[u][t];
    return 0;
}
static float used_of(int slot) {
    if (slot == EXT_SLOT) return g_ext_used_g;
    int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
    if (u >= 0 && u < AMS_MAX_UNITS && t >= 0 && t < AMS_MAX_TRAYS) return g_tray_used_g[u][t];
    return 0;
}
static void set_cap(int slot, float v) {
    if (v < 0) v = 0;
    if (slot == EXT_SLOT) { g_ext_capacity_g = v; return; }
    int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
    if (u >= 0 && u < AMS_MAX_UNITS && t >= 0 && t < AMS_MAX_TRAYS) g_tray_capacity_g[u][t] = v;
}
static void set_used(int slot, float v) {
    if (v < 0) v = 0;
    if (slot == EXT_SLOT) { g_ext_used_g = v; return; }
    int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
    if (u >= 0 && u < AMS_MAX_UNITS && t >= 0 && t < AMS_MAX_TRAYS) g_tray_used_g[u][t] = v;
}

// --- persistence ---------------------------------------------------------
void filament_save() {
    FILE* f = fopen(WEIGHTS_PATH, "w");
    if (!f) { Serial.println("WEIGHTS: save failed"); return; }
    fprintf(f, "low=%d\n", g_low_threshold_g);
    if (g_ext_capacity_g > 0) {
        fprintf(f, "ecap=%.1f\n", g_ext_capacity_g);
        fprintf(f, "eused=%.1f\n", g_ext_used_g);
        if (g_ext_price > 0) fprintf(f, "eprice=%.2f\n", g_ext_price);
    }
    for (int u = 0; u < AMS_MAX_UNITS; u++)
        for (int t = 0; t < AMS_MAX_TRAYS; t++)
            if (g_tray_capacity_g[u][t] > 0) {
                fprintf(f, "cap_%d_%d=%.1f\n", u, t, g_tray_capacity_g[u][t]);
                fprintf(f, "used_%d_%d=%.1f\n", u, t, g_tray_used_g[u][t]);
                if (g_tray_price[u][t] > 0) fprintf(f, "price_%d_%d=%.2f\n", u, t, g_tray_price[u][t]);
            }
    fclose(f);
}

void filament_track_init() {
    FILE* f = fopen(WEIGHTS_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        float val = (float)atof(eq + 1);
        int u, t;
        if (!strcmp(key, "low")) g_low_threshold_g = (int)val;
        else if (!strcmp(key, "ecap")) g_ext_capacity_g = val;
        else if (!strcmp(key, "eused")) g_ext_used_g = val;
        else if (!strcmp(key, "eprice")) g_ext_price = val;
        else if (sscanf(key, "cap_%d_%d", &u, &t) == 2 && u < AMS_MAX_UNITS && t < AMS_MAX_TRAYS) g_tray_capacity_g[u][t] = val;
        else if (sscanf(key, "used_%d_%d", &u, &t) == 2 && u < AMS_MAX_UNITS && t < AMS_MAX_TRAYS) g_tray_used_g[u][t] = val;
        else if (sscanf(key, "price_%d_%d", &u, &t) == 2 && u < AMS_MAX_UNITS && t < AMS_MAX_TRAYS) g_tray_price[u][t] = val;
    }
    fclose(f);
}

// --- weigh-flow ----------------------------------------------------------
void filament_weigh_assign(int slot, float measured_g, float empty_g) {
    float filament = measured_g - empty_g;
    if (filament < 0) filament = 0;
    set_cap(slot, filament);
    set_used(slot, 0);          // a fresh, just-weighed roll
    filament_save();
    Serial.printf("WEIGHTS: slot %d = %.1fg filament (weighed %.1f - empty %.1f)\n",
                  slot, filament, measured_g, empty_g);
}

// No roll in this slot anymore: drop its capacity/used/price so nothing is tracked
// or counted down for it (filament_remaining() then reports -1 = not weighed).
void filament_clear_slot(int slot) {
    set_cap(slot, 0);
    set_used(slot, 0);
    if (slot == EXT_SLOT) g_ext_price = 0;
    else {
        int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
        if (u >= 0 && u < AMS_MAX_UNITS && t >= 0 && t < AMS_MAX_TRAYS) g_tray_price[u][t] = 0;
    }
    filament_save();
    Serial.printf("WEIGHTS: slot %d cleared (no roll)\n", slot);
}

// --- live consumption ----------------------------------------------------
// Track the current print: capture the active slot's committed `used` when the
// print (file/slot) starts, then set used = base + total*progress each tick so
// the Filament screen's remaining ticks down. On stop, persist whatever we had.
static char  g_cur_file[128] = "";
static int   g_cur_slot = -1;
static float g_base_used = 0;

// Finished prints waiting for the cloud relay's actual weight, oldest-first. We
// record the slot and the grams the estimate applied for that print (0 when the
// file never parsed, so there was no estimate). filament_reconcile_actual() pops
// these and corrects the estimate to the real figure.
struct FinishedPrint { int slot; float estimated_g; };
#define FINISHED_MAX 4
static FinishedPrint g_finished[FINISHED_MAX];
static int g_finished_head = 0, g_finished_count = 0;

static void push_finished(int slot, float estimated_g) {
    int i = (g_finished_head + g_finished_count) % FINISHED_MAX;
    if (g_finished_count == FINISHED_MAX) {   // full: drop the oldest
        g_finished_head = (g_finished_head + 1) % FINISHED_MAX;
    } else {
        g_finished_count++;
    }
    g_finished[i] = { slot, estimated_g };
}

void filament_track_loop() {
    PrinterStatus& s = g_printer_status;
    bool printing = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
    int slot = s.active_tray_now;                 // 254 ext, else u*4+t, -1/255 none
    float total = gcode_view_ready() ? gcode_view_filament_g() : 0.0f;

    // Track any print on a slot that has a capacity (weighed or a typed roll
    // weight) - even when the gcode total isn't known, so the relay can still
    // attribute the finish. The estimate below only runs once the total is known.
    bool tracking = printing && s.gcode_file[0] && slot >= 0 && slot != 255 &&
                    (slot == EXT_SLOT || (slot / AMS_MAX_TRAYS) < AMS_MAX_UNITS) &&
                    cap_of(slot) > 0;

    if (tracking) {
        if (strcmp(g_cur_file, s.gcode_file) != 0 || g_cur_slot != slot) {
            strncpy(g_cur_file, s.gcode_file, sizeof(g_cur_file) - 1);
            g_cur_file[sizeof(g_cur_file) - 1] = 0;
            g_cur_slot = slot;
            g_base_used = used_of(slot);          // consumption already committed before this print
        }
        if (total > 0) {                          // only estimate once we know the total
            float pct = s.progress_pct; if (pct < 0) pct = 0; if (pct > 100) pct = 100;
            set_used(slot, g_base_used + total * pct / 100.0f);
        }
    } else if (g_cur_slot >= 0 && !printing) {
        // print ended (finished or cancelled): hand the relay the estimate we
        // applied (used - base, i.e. 0 if the total never parsed) so it can
        // correct to the real weight, then persist.
        push_finished(g_cur_slot, used_of(g_cur_slot) - g_base_used);
        filament_save();
        g_cur_slot = -1;
        g_cur_file[0] = 0;
    }
}

bool filament_reconcile_actual(float actual_g) {
    if (actual_g <= 0 || g_finished_count == 0) return false;
    FinishedPrint fp = g_finished[g_finished_head];
    g_finished_head = (g_finished_head + 1) % FINISHED_MAX;
    g_finished_count--;
    // used already holds base + estimated; swap the estimate for the real figure.
    float corrected = used_of(fp.slot) - fp.estimated_g + actual_g;
    if (corrected < 0) corrected = 0;
    set_used(fp.slot, corrected);
    filament_save();
    Serial.printf("CLOUD: reconciled slot %d - estimate %.1fg -> actual %.1fg\n",
                  fp.slot, fp.estimated_g, actual_g);
    return true;
}

// --- queries -------------------------------------------------------------
float filament_remaining(int slot) {
    float cap = cap_of(slot);
    if (cap <= 0) return -1;
    float r = cap - used_of(slot);
    return r < 0 ? 0 : r;
}
bool filament_slot_low(int slot) {
    float r = filament_remaining(slot);
    return r >= 0 && r < (float)g_low_threshold_g;
}
bool filament_any_low() {
    if (filament_slot_low(EXT_SLOT)) return true;
    for (int u = 0; u < AMS_MAX_UNITS; u++)
        for (int t = 0; t < AMS_MAX_TRAYS; t++)
            if (filament_slot_low(u * AMS_MAX_TRAYS + t)) return true;
    return false;
}

float filament_shortfall() {
    PrinterStatus& s = g_printer_status;
    bool printing = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
    if (!printing) return -1;
    int slot = s.active_tray_now;
    if (slot < 0 || slot == 255) return -1;
    if (!(slot == EXT_SLOT || (slot / AMS_MAX_TRAYS) < AMS_MAX_UNITS)) return -1;
    float total = gcode_view_ready() ? gcode_view_filament_g() : 0.0f;
    if (total <= 0) return -1;
    float have = filament_remaining(slot);          // -1 if the slot wasn't weighed
    if (have < 0) return -1;
    float pct = (float)s.progress_pct; if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    float need = total * (100.0f - pct) / 100.0f;    // filament still to be printed
    float shortfall = need - have;
    return shortfall > 0 ? shortfall : 0;
}

void filament_set_price(int slot, float price_kg) {
    if (price_kg < 0) price_kg = 0;
    if (slot == EXT_SLOT) g_ext_price = price_kg;
    else {
        int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
        if (u >= 0 && u < AMS_MAX_UNITS && t >= 0 && t < AMS_MAX_TRAYS) g_tray_price[u][t] = price_kg;
    }
    filament_save();
}
float filament_price(int slot) {
    if (slot == EXT_SLOT) return g_ext_price;
    int u = slot / AMS_MAX_TRAYS, t = slot % AMS_MAX_TRAYS;
    if (u >= 0 && u < AMS_MAX_UNITS && t >= 0 && t < AMS_MAX_TRAYS) return g_tray_price[u][t];
    return 0;
}

float filament_capacity(int slot) { return cap_of(slot); }

bool filament_live_cost(float* grams_out, float* eur_out) {
    PrinterStatus& s = g_printer_status;
    bool printing = (strcmp(s.gcode_state, "RUNNING") == 0 || strcmp(s.gcode_state, "PAUSE") == 0);
    if (!printing) return false;
    float total = gcode_view_ready() ? gcode_view_filament_g() : 0.0f;
    if (total <= 0) return false;
    float pct = (float)s.progress_pct; if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    float g = total * pct / 100.0f;
    int slot = s.active_tray_now;
    float price = (slot >= 0 && slot != 255) ? filament_price(slot) : 0.0f;
    if (grams_out) *grams_out = g;
    if (eur_out)   *eur_out = g * price / 1000.0f;
    return true;
}
