#pragma once
#ifndef SPOOL_DB_H
#define SPOOL_DB_H

#include <cstdint>

// A small on-tablet spool library: pre-fill your rolls once, then "load" one
// into an AMS slot - which sets our weight tracking (capacity = remaining) and
// pushes the material/colour/temps to the printer's AMS over MQTT.
// Persisted to /sdcard/pandatouch_spools.conf.

#define SPOOL_MAX 48

struct Spool {
    char name[40];
    char material[12];   // PLA, PETG, ABS, TPU, ...
    char code[16];       // Bambu tray_info_idx (optional; blank -> derived)
    char note[80];
    uint32_t color;      // RGB
    float remaining_g;   // filament grams left on the roll
    float empty_g;       // empty-spool weight (for re-weighing)
    int nmin, nmax;      // nozzle temp range
};

extern Spool g_spools[SPOOL_MAX];
extern int   g_spool_count;

void spool_db_load();
void spool_db_save();

// Add (idx < 0) or update (idx) a spool. Returns the row index, or -1 on error.
int  spool_upsert(int idx, const Spool& s);
void spool_delete(int idx);

// Fill code/temps from the material when the user left them blank/zero.
void spool_material_defaults(const char* material, const char** code, int* nmin, int* nmax);

// Load spool `idx` into `slot` (254 = external): set weight tracking + (for AMS
// slots) push ams_filament_setting to the printer. Call on the MAIN thread.
void spool_load_to_slot(int idx, int slot);

#endif
