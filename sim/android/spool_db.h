#pragma once
#ifndef SPOOL_DB_H
#define SPOOL_DB_H

#include <cstdint>

// A small on-tablet spool library: pre-fill your rolls once, then "load" one
// into an AMS slot - which sets our weight tracking (capacity = remaining) and
// pushes the material/colour/temps to the printer's AMS over MQTT.
// Persisted to /sdcard/filatrack_spools.conf.

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
    float price_kg;      // price per kg (for cost tracking), 0 = unknown
    int slot;            // AMS slot this roll is loaded in: -1 none, 254 external,
                         // else unit*AMS_MAX_TRAYS + tray. Managed by load/clear.
};

extern Spool g_spools[SPOOL_MAX];
extern int   g_spool_count;

// A small library of empty-spool weights (per brand/type) to pick from when
// weighing or creating a roll, so you don't retype the empty weight each time.
#define EMPTY_MAX 24
struct EmptySpool {
    char name[40];
    float weight_g;
};
extern EmptySpool g_empties[EMPTY_MAX];
extern int        g_empty_count;

void empty_db_load();
void empty_db_save();
int  empty_upsert(int idx, const EmptySpool& e);   // idx<0 = add
void empty_delete(int idx);

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

// Mark `slot` empty (no roll): clears our weight tracking for it and unlinks any
// roll from it (saving that roll's final remaining grams first). Leaves the
// printer's own AMS state alone (that follows the physical filament sensor).
void spool_clear_slot(int slot);

// Live remaining grams for a roll: if it's loaded in an AMS slot, the slot's
// tracked value (which ticks down while printing); else the stored remaining_g.
float spool_live_grams(const Spool& s);
// Human label for a slot: "AMS1 T2" / "Extern" / "" (unassigned) into out.
void  spool_slot_label(int slot, char* out, int len);

// Material of the roll currently loaded into `slot` (from our library), or ""
// if no roll is linked to that slot. Used to tag print-history entries.
void  spool_material_for_slot(int slot, char* out, int len);

// Call from the main loop: when a tracked roll is physically pulled from the AMS
// (tray reports empty for a while), auto-clear that slot like pressing "Leeg".
void  spool_autoclear_loop();

#endif
