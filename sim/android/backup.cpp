// Back-up / restore of the tablet's built-up data (see backup.h).
#include "backup.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

// The data files we protect. Order is stable so a backup blob is deterministic.
struct DataFile { const char* tag; const char* path; };
static const DataFile FILES[] = {
    {"spools",  "/sdcard/printward_spools.conf"},
    {"empties", "/sdcard/printward_empties.conf"},
    {"weights", "/sdcard/printward_weights.conf"},
    {"history", "/sdcard/printward_history.conf"},
    {"stats",   "/sdcard/printward_stats.conf"},
};
static const int NFILES = (int)(sizeof(FILES) / sizeof(FILES[0]));

// Read a whole file into a malloc'd, NUL-terminated buffer. *len_out = bytes.
static char* read_all(const char* path, long* len_out) {
    if (len_out) *len_out = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) n = 0;
    char* buf = (char*)malloc(n + 1);
    if (!buf) { fclose(f); return nullptr; }
    long got = (long)fread(buf, 1, n, f);
    fclose(f);
    if (got < 0) got = 0;
    buf[got] = 0;
    if (len_out) *len_out = got;
    return buf;
}

static bool write_all(const char* path, const char* data, long len) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    if (len > 0) fwrite(data, 1, len, f);
    fclose(f);
    return true;
}

char* backup_build(int* out_len) {
    if (out_len) *out_len = 0;
    size_t cap = 256;
    for (int i = 0; i < NFILES; i++) {
        struct stat st;
        if (stat(FILES[i].path, &st) == 0) cap += (size_t)st.st_size + 64;
    }
    char* out = (char*)malloc(cap);
    if (!out) return nullptr;
    int p = 0;
    p += snprintf(out + p, cap - p, "#PTBACKUP1\n");
    for (int i = 0; i < NFILES; i++) {
        p += snprintf(out + p, cap - p, "@@%s\n", FILES[i].tag);
        long fl = 0;
        char* fc = read_all(FILES[i].path, &fl);
        if (fc && fl > 0 && (size_t)(p + fl + 2) < cap) {
            memcpy(out + p, fc, fl);
            p += fl;
            if (out[p - 1] != '\n') out[p++] = '\n';
        }
        free(fc);
    }
    p += snprintf(out + p, cap - p, "@@end\n");
    out[p] = 0;
    if (out_len) *out_len = p;
    return out;
}

int backup_apply(const char* data) {
    if (!data) return 0;
    int restored = 0;
    const char* p = data;
    // Skip the magic first line.
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
    while (*p) {
        if (p[0] == '@' && p[1] == '@') {
            const char* tagstart = p + 2;
            const char* nl = strchr(tagstart, '\n');
            if (!nl) break;
            char tag[16];
            int tl = (int)(nl - tagstart);
            if (tl > 15) tl = 15;
            memcpy(tag, tagstart, tl);
            tag[tl] = 0;
            if (!strcmp(tag, "end")) break;
            const char* body = nl + 1;
            // Body runs until the next "@@" at the start of a line, or the end.
            const char* q = body;
            while (*q) {
                if (q[0] == '@' && q[1] == '@' && (q == body || q[-1] == '\n')) break;
                q++;
            }
            long blen = (long)(q - body);
            for (int i = 0; i < NFILES; i++) {
                if (!strcmp(tag, FILES[i].tag)) {
                    if (write_all(FILES[i].path, body, blen)) restored++;
                    break;
                }
            }
            p = q;
        } else {
            const char* nl = strchr(p, '\n');
            if (!nl) break;
            p = nl + 1;
        }
    }
    return restored;
}

// --- one-time migration from the old PandaTouch / FilaTrack file names --------
// Printward is the third name of this project (PandaTouch -> FilaTrack -> Printward).
// On first run, adopt any data left under an old name so nobody starts empty. The
// direct predecessor (FilaTrack) is listed first, so it wins if both ever coexist.
void migrate_legacy_data() {
    static const struct { const char* from; const char* to; } M[] = {
        {"/sdcard/filatrack.conf",               "/sdcard/printward.conf"},
        {"/sdcard/filatrack_spools.conf",        "/sdcard/printward_spools.conf"},
        {"/sdcard/filatrack_empties.conf",       "/sdcard/printward_empties.conf"},
        {"/sdcard/filatrack_weights.conf",       "/sdcard/printward_weights.conf"},
        {"/sdcard/filatrack_history.conf",       "/sdcard/printward_history.conf"},
        {"/sdcard/filatrack_stats.conf",         "/sdcard/printward_stats.conf"},
        {"/sdcard/filatrack_backup_state.conf",  "/sdcard/printward_backup_state.conf"},
        {"/sdcard/filatrack_lownotify.conf",     "/sdcard/printward_lownotify.conf"},
        {"/sdcard/filatrack_lang_de.conf",       "/sdcard/printward_lang_de.conf"},
        {"/sdcard/pandatouch.conf",              "/sdcard/printward.conf"},
        {"/sdcard/pandatouch_spools.conf",       "/sdcard/printward_spools.conf"},
        {"/sdcard/pandatouch_empties.conf",      "/sdcard/printward_empties.conf"},
        {"/sdcard/pandatouch_weights.conf",      "/sdcard/printward_weights.conf"},
        {"/sdcard/pandatouch_history.conf",      "/sdcard/printward_history.conf"},
        {"/sdcard/pandatouch_stats.conf",        "/sdcard/printward_stats.conf"},
        {"/sdcard/pandatouch_backup_state.conf", "/sdcard/printward_backup_state.conf"},
        {"/sdcard/pandatouch_lownotify.conf",    "/sdcard/printward_lownotify.conf"},
    };
    int moved = 0;
    for (int i = 0; i < (int)(sizeof(M) / sizeof(M[0])); i++) {
        struct stat st;
        if (stat(M[i].to, &st) == 0) continue;     // already migrated - never clobber
        if (stat(M[i].from, &st) != 0) continue;   // nothing there to migrate
        if (rename(M[i].from, M[i].to) == 0) { moved++; continue; }
        // rename() can fail across some Android mounts: fall back to a copy.
        long len = 0;
        char* d = read_all(M[i].from, &len);
        if (d) { if (write_all(M[i].to, d, len)) moved++; free(d); }
    }
    if (moved) Serial.printf("MIGRATE: %d legacy data file(s) -> Printward\n", moved);
}

// --- "is your data actually safe?" bookkeeping ----------------------------
#define BSTATE_PATH "/sdcard/printward_backup_state.conf"

static long bstate_get(const char* key) {
    FILE* f = fopen(BSTATE_PATH, "r");
    if (!f) return 0;
    char line[64];
    long v = 0;
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        if (!strcmp(line, key)) { v = atol(eq + 1); break; }
    }
    fclose(f);
    return v;
}

static void bstate_set(const char* key, long val) {
    long dl = bstate_get("last_dl"), rm = bstate_get("last_remind");
    if (!strcmp(key, "last_dl")) dl = val; else rm = val;
    FILE* f = fopen(BSTATE_PATH, "w");
    if (!f) return;
    fprintf(f, "last_dl=%ld\nlast_remind=%ld\n", dl, rm);
    fclose(f);
}

void backup_mark_downloaded() { bstate_set("last_dl", (long)time(nullptr)); }
long backup_last_download()   { return bstate_get("last_dl"); }
long backup_last_remind()     { return bstate_get("last_remind"); }
void backup_mark_remind()     { bstate_set("last_remind", (long)time(nullptr)); }

long backup_seconds_since_dl() {
    long dl = backup_last_download();
    if (dl <= 0) return -1;
    long d = (long)time(nullptr) - dl;
    return d < 0 ? 0 : d;
}

// --- removable SD copy ----------------------------------------------------
// mkdir -p
static bool ensure_dir(const char* path) {
    char tmp[192];
    strncpy(tmp, path, sizeof(tmp) - 1); tmp[sizeof(tmp) - 1] = 0;
    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        mkdir(tmp, 0775);
        *p = '/';
    }
    mkdir(tmp, 0775);
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Android 5.1 only lets an app write its OWN folder on secondary storage, so we
// aim at <card>/Android/data/<pkg>/files rather than the card root.
const char* backup_sd_dir() {
    static char found[192] = "";
    static bool probed = false;
    if (probed) return found;
    probed = true;

    static const char* roots[] = {
        "/storage/extSdCard", "/storage/sdcard1", "/storage/external_SD",
        "/storage/MicroSD", "/mnt/extSdCard",
    };
    for (int i = 0; i < (int)(sizeof(roots) / sizeof(roots[0])); i++) {
        struct stat st;
        if (stat(roots[i], &st) != 0 || !S_ISDIR(st.st_mode)) continue;   // no card here
        char dir[192];
        snprintf(dir, sizeof(dir), "%s/Android/data/nl.printward.app/files", roots[i]);
        if (!ensure_dir(dir)) continue;
        char probe[224];
        snprintf(probe, sizeof(probe), "%s/.pt_write_test", dir);
        FILE* f = fopen(probe, "w");
        if (!f) continue;                     // mounted but not writable -> skip
        fclose(f);
        remove(probe);
        strncpy(found, dir, sizeof(found) - 1); found[sizeof(found) - 1] = 0;
        Serial.printf("BACKUP: SD copy -> %s\n", found);
        return found;
    }
    return found;   // "" = no usable card
}

void backup_auto_loop() {
    // Throttle: a filesystem scan every ~15 s is plenty; the data changes slowly.
    static unsigned long next = 0;
    static long last_mtime = 0;
    unsigned long now = millis();
    if (next != 0 && now < next) return;
    next = now + 15000;

    long newest = 0;
    for (int i = 0; i < NFILES; i++) {
        struct stat st;
        if (stat(FILES[i].path, &st) == 0 && (long)st.st_mtime > newest) newest = (long)st.st_mtime;
    }
    if (newest == 0 || newest == last_mtime) return;   // nothing (new) to snapshot
    last_mtime = newest;

    mkdir("/sdcard/printward_backup", 0777);
    for (int i = 0; i < NFILES; i++) {
        long fl = 0;
        char* fc = read_all(FILES[i].path, &fl);
        if (fc) {
            char dst[128];
            snprintf(dst, sizeof(dst), "/sdcard/printward_backup/%s.conf", FILES[i].tag);
            write_all(dst, fc, fl);
            free(fc);
        }
    }
    Serial.println("BACKUP: snapshot -> /sdcard/printward_backup");

    // Also drop a single-file copy on a removable card when one is present. That
    // copy survives a factory reset and can be pulled out of a dead tablet, which
    // the /sdcard snapshot cannot.
    const char* sd = backup_sd_dir();
    if (sd[0]) {
        int len = 0;
        char* blob = backup_build(&len);
        if (blob && len > 0) {
            char dst[224];
            snprintf(dst, sizeof(dst), "%s/printward-backup.ptb", sd);
            if (write_all(dst, blob, len)) Serial.printf("BACKUP: SD copy written (%d B)\n", len);
        }
        free(blob);
    }
}
