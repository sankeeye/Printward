#pragma once
#ifndef STATS_H
#define STATS_H

// Lifetime print statistics: number of finished prints and total filament used
// (grams, from the gcode total when a print finishes). Persisted to
// /sdcard/pandatouch_stats.conf.

extern int   g_stat_prints;
extern float g_stat_grams;

void stats_init();   // load persisted totals
void stats_loop();   // add a print's filament when it finishes

#endif
