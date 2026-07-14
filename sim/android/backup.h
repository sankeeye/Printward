#pragma once
#ifndef BACKUP_H
#define BACKUP_H

// Back-up & restore of all the data the tablet builds up over time: the spool
// library, empty-spool weights, per-slot filament weights, print history and
// lifetime statistics. These live only on /sdcard, so a factory reset / app
// reinstall would otherwise lose everything.
//
// The printer config (pandatouch.conf) is intentionally NOT included: it holds
// the printer access code (a secret) and is trivially re-entered on the setup
// screen, so it never lands in a downloadable/importable file.

// Periodic on-device snapshot: when any data file changed, copy them all to
// /sdcard/ptbackup/ (a rolling local safety net). Call from the main loop;
// internally throttled, so calling every iteration is cheap.
void backup_auto_loop();

// Build one combined backup blob of all data files. malloc'd, NUL-terminated,
// length in *out_len. Caller free()s. nullptr on allocation failure.
char* backup_build(int* out_len);

// Apply a backup blob previously made by backup_build(): overwrite each data
// file with its section. Returns the number of files restored. The caller must
// reload the in-memory modules afterwards (spool_db_load, history_init, ...).
int backup_apply(const char* data);

#endif
