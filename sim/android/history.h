#pragma once
#ifndef HISTORY_H
#define HISTORY_H

// Recent print log: one record per finished/failed print (date, name, filament
// grams, cost, result), newest first. Persisted to
// /sdcard/printward_history.conf. Cost uses the active slot's EUR/kg.

#define HIST_MAX 100

struct PrintRec {
    char  when[20];   // "dd-mm HH:MM" (display)
    char  name[64];
    float grams;
    float cost;       // EUR
    int   ok;         // 1 = finished, 0 = failed
    char  file[64];   // printed .3mf (in /cache) for an on-demand model preview
    int   arch;       // 1 = archived (hidden from the active list)
    char  mat[12];    // material used (PLA/PETG/...), for the per-material totals
    long  ts;         // unix epoch of the finish (0 = unknown/old entry) - timeline
    int   mins;       // wall-clock print duration in minutes (0 = unknown)
};

extern PrintRec g_history[HIST_MAX];
extern int      g_hist_count;
extern float    g_hist_total_cost;

void history_init();
void history_loop();
void history_set_arch(int idx, int v);   // archive (1) / restore (0) an entry
void history_remove(int idx);            // permanently delete an entry

#endif
