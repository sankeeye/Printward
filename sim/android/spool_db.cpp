// On-tablet spool library + "load into AMS slot" (weight tracking + MQTT AMS).
#include "spool_db.h"
#include "filament_track.h"   // filament_weigh_assign()
#include "bambu_mqtt.h"       // bambu_cmd_set_ams_filament(), AMS_MAX_*
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <strings.h>

Spool g_spools[SPOOL_MAX];
int   g_spool_count = 0;
EmptySpool g_empties[EMPTY_MAX];
int        g_empty_count = 0;

#define SPOOLS_PATH  "/sdcard/pandatouch_spools.conf"
#define EMPTIES_PATH "/sdcard/pandatouch_empties.conf"

// Bambu generic filament codes + typical temps per material. Best-effort - the
// per-spool `code` field overrides these when the user fills it in.
void spool_material_defaults(const char* m, const char** code, int* nmin, int* nmax) {
    struct Row { const char* m; const char* c; int a, b; };
    static const Row t[] = {
        {"PLA", "GFL99", 190, 230}, {"PETG", "GFG99", 230, 260}, {"ABS", "GFB99", 240, 270},
        {"TPU", "GFU99", 210, 240}, {"ASA", "GFB98", 240, 270}, {"PC", "GFC99", 260, 280},
        {"PA", "GFN99", 260, 290}, {"PVA", "GFS99", 190, 230},
    };
    for (const Row& r : t)
        if (strcasecmp(m, r.m) == 0) { *code = r.c; *nmin = r.a; *nmax = r.b; return; }
    *code = "GFL99"; *nmin = 190; *nmax = 230;
}

// --- persistence: one pipe-delimited line per spool ----------------------
static void sanitize(char* s) {
    for (; *s; s++) if (*s == '|' || *s == '\n' || *s == '\r') *s = ' ';
}

void spool_db_save() {
    FILE* f = fopen(SPOOLS_PATH, "w");
    if (!f) { Serial.println("SPOOLS: save failed"); return; }
    for (int i = 0; i < g_spool_count; i++) {
        Spool s = g_spools[i];
        sanitize(s.name); sanitize(s.material); sanitize(s.code); sanitize(s.note);
        fprintf(f, "%s|%s|%s|%06X|%.1f|%.1f|%d|%d|%s|%.2f|%d\n",
                s.name, s.material, s.code, (unsigned)(s.color & 0xFFFFFF),
                s.remaining_g, s.empty_g, s.nmin, s.nmax, s.note, s.price_kg, s.slot);
    }
    fclose(f);
}

static char* field(char** p) {
    char* start = *p;
    char* bar = strchr(start, '|');
    if (bar) { *bar = 0; *p = bar + 1; }
    else *p = start + strlen(start);
    return start;
}

void spool_db_load() {
    g_spool_count = 0;
    FILE* f = fopen(SPOOLS_PATH, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f) && g_spool_count < SPOOL_MAX) {
        char* nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        if (!line[0]) continue;
        char* p = line;
        Spool s; memset(&s, 0, sizeof(s));
        strncpy(s.name, field(&p), sizeof(s.name) - 1);
        strncpy(s.material, field(&p), sizeof(s.material) - 1);
        strncpy(s.code, field(&p), sizeof(s.code) - 1);
        s.color = (uint32_t)strtoul(field(&p), nullptr, 16);
        s.remaining_g = (float)atof(field(&p));
        s.empty_g = (float)atof(field(&p));
        s.nmin = atoi(field(&p));
        s.nmax = atoi(field(&p));
        strncpy(s.note, field(&p), sizeof(s.note) - 1);
        s.price_kg = (float)atof(field(&p));   // absent in old files -> 0
        char* slotf = field(&p);               // 11th field, absent in old files
        s.slot = (slotf && slotf[0]) ? atoi(slotf) : -1;
        g_spools[g_spool_count++] = s;
    }
    fclose(f);
    Serial.printf("SPOOLS: loaded %d\n", g_spool_count);
}

int spool_upsert(int idx, const Spool& s) {
    int keep = -1;   // slot is managed by load/clear, never by the editor
    if (idx < 0) {
        if (g_spool_count >= SPOOL_MAX) return -1;
        idx = g_spool_count++;
    } else if (idx >= g_spool_count) {
        return -1;
    } else {
        keep = g_spools[idx].slot;
    }
    g_spools[idx] = s;
    g_spools[idx].slot = keep;
    spool_db_save();
    return idx;
}

void spool_delete(int idx) {
    if (idx < 0 || idx >= g_spool_count) return;
    for (int i = idx; i < g_spool_count - 1; i++) g_spools[i] = g_spools[i + 1];
    g_spool_count--;
    spool_db_save();
}

// --- empty-spool library -------------------------------------------------
void empty_db_save() {
    FILE* f = fopen(EMPTIES_PATH, "w");
    if (!f) { Serial.println("EMPTIES: save failed"); return; }
    for (int i = 0; i < g_empty_count; i++) {
        EmptySpool e = g_empties[i];
        sanitize(e.name);
        fprintf(f, "%s|%.1f\n", e.name, e.weight_g);
    }
    fclose(f);
}
void empty_db_load() {
    g_empty_count = 0;
    FILE* f = fopen(EMPTIES_PATH, "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f) && g_empty_count < EMPTY_MAX) {
        char* nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        if (!line[0]) continue;
        char* p = line;
        EmptySpool e; memset(&e, 0, sizeof(e));
        strncpy(e.name, field(&p), sizeof(e.name) - 1);
        e.weight_g = (float)atof(field(&p));
        g_empties[g_empty_count++] = e;
    }
    fclose(f);
    Serial.printf("EMPTIES: loaded %d\n", g_empty_count);
}
int empty_upsert(int idx, const EmptySpool& e) {
    if (idx < 0) { if (g_empty_count >= EMPTY_MAX) return -1; idx = g_empty_count++; }
    else if (idx >= g_empty_count) return -1;
    g_empties[idx] = e;
    empty_db_save();
    return idx;
}
void empty_delete(int idx) {
    if (idx < 0 || idx >= g_empty_count) return;
    for (int i = idx; i < g_empty_count - 1; i++) g_empties[i] = g_empties[i + 1];
    g_empty_count--;
    empty_db_save();
}

void spool_load_to_slot(int idx, int slot) {
    if (idx < 0 || idx >= g_spool_count) return;
    Spool& s = g_spools[idx];

    // this slot now holds THIS roll: unlink it from any other roll first
    for (int i = 0; i < g_spool_count; i++)
        if (i != idx && g_spools[i].slot == slot) g_spools[i].slot = -1;
    s.slot = slot;

    // 1. weight tracking. If the slot is already tracking (it has been printing,
    // or was assigned before the roll<->slot link existed), adopt its current
    // remaining as this roll's weight instead of resetting - so linking a roll
    // never inflates back to the entered weight or loses the already-counted
    // grams. A fresh/empty slot takes the roll's own weight. (Press "Leeg" first
    // when you physically swap in a different roll.)
    float tracked = filament_remaining(slot);
    if (tracked >= 0) s.remaining_g = tracked;                        // adopt live tracked weight
    else              filament_weigh_assign(slot, s.remaining_g, 0);  // fresh slot -> roll weight
    filament_set_price(slot, s.price_kg);   // remember EUR/kg for cost tracking

    // 2. push to the printer's AMS (external spool 254 is set differently - skip)
    if (slot != 254) {
        int ams = slot / AMS_MAX_TRAYS, tray = slot % AMS_MAX_TRAYS;
        const char* code = s.code;
        int nmin = s.nmin, nmax = s.nmax;
        if (!code[0] || nmin <= 0 || nmax <= 0) {
            const char* dc; int da, db;
            spool_material_defaults(s.material, &dc, &da, &db);
            if (!code[0]) code = dc;
            if (nmin <= 0) nmin = da;
            if (nmax <= 0) nmax = db;
        }
        bambu_cmd_set_ams_filament(ams, tray, code, s.color, nmin, nmax, s.material);
        Serial.printf("SPOOLS: loaded '%s' -> AMS%d T%d (%s)\n", s.name, ams + 1, tray + 1, code);
    } else {
        Serial.printf("SPOOLS: loaded '%s' -> externe spoel (alleen tracking)\n", s.name);
    }
    spool_db_save();   // persist the roll<->slot link
}

void spool_clear_slot(int slot) {
    float r = filament_remaining(slot);   // save the roll's final grams before clearing
    for (int i = 0; i < g_spool_count; i++)
        if (g_spools[i].slot == slot) {
            if (r >= 0) g_spools[i].remaining_g = r;
            g_spools[i].slot = -1;
        }
    filament_clear_slot(slot);            // no roll: stop tracking/counting this slot
    spool_db_save();
    Serial.printf("SPOOLS: slot %d cleared (no roll)\n", slot);
}

float spool_live_grams(const Spool& s) {
    if (s.slot >= 0) {
        float r = filament_remaining(s.slot);
        if (r >= 0) return r;
    }
    return s.remaining_g;
}
void spool_slot_label(int slot, char* out, int len) {
    if (slot < 0) { if (len) out[0] = 0; return; }
    if (slot == 254) snprintf(out, len, "Extern");
    else snprintf(out, len, "AMS%d T%d", slot / AMS_MAX_TRAYS + 1, slot % AMS_MAX_TRAYS + 1);
}
