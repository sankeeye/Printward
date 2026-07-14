// Back-up / restore of the tablet's built-up data (see backup.h).
#include "backup.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

// The data files we protect. Order is stable so a backup blob is deterministic.
struct DataFile { const char* tag; const char* path; };
static const DataFile FILES[] = {
    {"spools",  "/sdcard/pandatouch_spools.conf"},
    {"empties", "/sdcard/pandatouch_empties.conf"},
    {"weights", "/sdcard/pandatouch_weights.conf"},
    {"history", "/sdcard/pandatouch_history.conf"},
    {"stats",   "/sdcard/pandatouch_stats.conf"},
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

    mkdir("/sdcard/ptbackup", 0777);
    for (int i = 0; i < NFILES; i++) {
        long fl = 0;
        char* fc = read_all(FILES[i].path, &fl);
        if (fc) {
            char dst[128];
            snprintf(dst, sizeof(dst), "/sdcard/ptbackup/%s.conf", FILES[i].tag);
            write_all(dst, fc, fl);
            free(fc);
        }
    }
    Serial.println("BACKUP: snapshot -> /sdcard/ptbackup");
}
