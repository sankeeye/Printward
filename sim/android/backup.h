#pragma once
#ifndef BACKUP_H
#define BACKUP_H

// Back-up & restore of all the data the tablet builds up over time: the spool
// library, empty-spool weights, per-slot filament weights, print history and
// lifetime statistics. These live only on /sdcard, so a factory reset / app
// reinstall would otherwise lose everything.
//
// The printer config (filatrack.conf) is intentionally NOT included: it holds
// the printer access code (a secret) and is trivially re-entered on the setup
// screen, so it never lands in a downloadable/importable file.

// One-time rename of the pre-FilaTrack (PandaTouch) data files. MUST run once at
// startup BEFORE anything loads, otherwise an existing install would silently
// come up empty and its rolls/weights/history/stats would look lost.
void migrate_legacy_data();

// Periodic on-device snapshot: when any data file changed, copy them all to
// /sdcard/filatrack_backup/ (a rolling local safety net). Call from the main
// loop; internally throttled, so calling every iteration is cheap.
void backup_auto_loop();

// Build one combined backup blob of all data files. malloc'd, NUL-terminated,
// length in *out_len. Caller free()s. nullptr on allocation failure.
char* backup_build(int* out_len);

// Apply a backup blob previously made by backup_build(): overwrite each data
// file with its section. Returns the number of files restored. The caller must
// reload the in-memory modules afterwards (spool_db_load, history_init, ...).
int backup_apply(const char* data);

// --- "is your data actually safe?" bookkeeping ----------------------------
// A snapshot on /sdcard only survives an app reinstall, not a wipe or a dead
// tablet. What counts as safe is a backup that left the device, so we remember
// when /backup was last downloaded and nudge when that was too long ago.

void backup_mark_downloaded();   // call when /backup is served
long backup_last_download();     // epoch of the last download, 0 = never
long backup_seconds_since_dl();  // seconds since the last download, -1 = never

// Reminder throttle (so the ntfy nudge stays once-a-week, not once-a-loop).
long backup_last_remind();
void backup_mark_remind();

// Where the removable-SD copy goes, "" when no usable card is present. The
// snapshot in backup_auto_loop() writes there too - unlike /sdcard, a card
// survives a factory reset and can be pulled out of a dead tablet.
const char* backup_sd_dir();

#endif
