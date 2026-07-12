// Android-only G-code loader for the screensaver's live build-up view.
// FTP-download the current print's .3mf, unzip Metadata/plate_1.gcode (zlib is
// part of the Android NDK, linked with -lz), and parse the XY extrusion moves
// into per-layer segments. Blocking; run off the UI where a stall is fine.
#include "gcode_view.h"
#include "bambu_ftp.h"
#include <Arduino.h>      // Serial (logcat)
#include <zlib.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

// ---- downloaded .3mf (ZIP) buffer -------------------------------------------
static uint8_t* g_zip = nullptr;
static uint32_t g_zip_len = 0, g_zip_cap = 0;

// ---- parsed segments --------------------------------------------------------
#define GV_MAX_SEGS 300000
static GcodeSeg* g_segs = nullptr;
static int g_seg_count = 0;
static int g_max_layer = 0;
static int16_t g_minx = 0, g_miny = 0, g_maxx = 0, g_maxy = 0, g_maxz = 0;
static volatile bool g_ready = false;   // set last, read from the UI thread

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static bool zip_dl_cb(const uint8_t* d, unsigned int len, void*) {
    if (g_zip_len + len > g_zip_cap) {
        uint32_t cap = g_zip_cap ? g_zip_cap : 65536;
        while (cap < g_zip_len + len) cap *= 2;
        uint8_t* nb = (uint8_t*)realloc(g_zip, cap);
        if (!nb) return false;
        g_zip = nb; g_zip_cap = cap;
    }
    memcpy(g_zip + g_zip_len, d, len);
    g_zip_len += len;
    return true;
}

// Find Metadata/plate_1.gcode in the ZIP and inflate it. Returns a malloc'd,
// null-terminated buffer (+ length in *out_len), or nullptr.
static char* unzip_gcode(uint32_t* out_len) {
    *out_len = 0;
    if (!g_zip || g_zip_len < 22) return nullptr;

    int32_t eocd = -1;                        // End Of Central Directory record
    int32_t lim = (int32_t)g_zip_len - 22;
    for (int32_t i = lim; i >= 0 && i > lim - 65536; i--) {
        if (rd32(g_zip + i) == 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) return nullptr;
    uint32_t cd_off = rd32(g_zip + eocd + 16);

    const char* target = "Metadata/plate_1.gcode";
    size_t tlen = strlen(target);
    uint32_t p = cd_off;
    while (p + 46 <= g_zip_len && rd32(g_zip + p) == 0x02014b50) {
        uint16_t method = rd16(g_zip + p + 10);
        uint32_t comp   = rd32(g_zip + p + 20);
        uint32_t uncomp = rd32(g_zip + p + 24);
        uint16_t fnlen  = rd16(g_zip + p + 28);
        uint16_t exlen  = rd16(g_zip + p + 30);
        uint16_t cmlen  = rd16(g_zip + p + 32);
        uint32_t lho    = rd32(g_zip + p + 42);
        const char* fn  = (const char*)(g_zip + p + 46);
        if (fnlen == tlen && memcmp(fn, target, tlen) == 0) {
            if (lho + 30 > g_zip_len) return nullptr;
            uint16_t lfn = rd16(g_zip + lho + 26);
            uint16_t lex = rd16(g_zip + lho + 28);
            uint32_t data = lho + 30 + lfn + lex;
            if ((uint64_t)data + comp > g_zip_len) return nullptr;
            char* out = (char*)malloc((size_t)uncomp + 1);
            if (!out) { Serial.printf("GCODE: OOM allocating %u bytes for gcode\n", (unsigned)uncomp + 1); return nullptr; }
            if (method == 0) {
                memcpy(out, g_zip + data, uncomp);
            } else if (method == 8) {
                z_stream zs; memset(&zs, 0, sizeof(zs));
                if (inflateInit2(&zs, -15) != Z_OK) { free(out); return nullptr; }
                zs.next_in = (Bytef*)(g_zip + data); zs.avail_in = comp;
                zs.next_out = (Bytef*)out;           zs.avail_out = uncomp;
                int r = inflate(&zs, Z_FINISH);
                uncomp = (uint32_t)zs.total_out;
                inflateEnd(&zs);
                if (r != Z_STREAM_END && r != Z_OK) { free(out); return nullptr; }
            } else { free(out); return nullptr; }
            out[uncomp] = '\0';
            *out_len = uncomp;
            return out;
        }
        p += 46 + fnlen + exlen + cmlen;
    }
    return nullptr;
}

// Parse one number after a token letter (e.g. after 'X'); returns true if found.
static bool tok(const char* s, const char* end, char letter, float* val) {
    for (const char* c = s; c < end; c++) {
        if (*c == letter) { *val = (float)atof(c + 1); return true; }
        if (*c == ';') break;
    }
    return false;
}

static int cmp_i16(const void* a, const void* b) {
    return (int)(*(const int16_t*)a) - (int)(*(const int16_t*)b);
}

static float g_minf, g_maxf_x, g_minf_y, g_maxf_y;   // running bounds (mm)
static float g_filament_g = 0;   // total filament for this print (grams), 0 = unknown

// The slicer writes the filament total as a comment, e.g.
//   ; total filament weight [g] : 15.67
//   ; filament used [g] = 10.5,5.2     (multi-material -> summed)
// Scan the whole gcode for it so the weight tracker can estimate consumption.
static float scan_filament_g(const char* g, uint32_t len) {
    const char* keys[] = {"total filament weight [g]", "filament used [g]"};
    const char* e = g + len;
    for (int k = 0; k < 2; k++) {
        size_t kl = strlen(keys[k]);
        const char* p = (const char*)memmem(g, len, keys[k], kl);
        if (!p) continue;
        const char* q = p + kl;
        while (q < e && *q != '=' && *q != ':' && *q != '\n') q++;   // skip to = or :
        if (q >= e || *q == '\n') continue;
        q++;
        float sum = 0; bool any = false;
        while (q < e && *q != '\n') {
            while (q < e && (*q == ' ' || *q == ',' || *q == '\t')) q++;
            if (q < e && (*q == '.' || (*q >= '0' && *q <= '9'))) {
                sum += (float)atof(q); any = true;
                while (q < e && *q != ',' && *q != '\n') q++;
            } else break;
        }
        if (any && sum > 0) return sum;
    }
    return 0;
}

static inline void add_seg(float x1, float y1, float x2, float y2, float z, int layer) {
    float dx = x2 - x1, dy = y2 - y1;
    if (dx * dx + dy * dy < 0.01f) return;               // skip < 0.1 mm
    if (g_seg_count >= GV_MAX_SEGS) return;
    GcodeSeg& s = g_segs[g_seg_count++];
    s.x1 = (int16_t)(x1 * 10); s.y1 = (int16_t)(y1 * 10);
    s.x2 = (int16_t)(x2 * 10); s.y2 = (int16_t)(y2 * 10);
    s.z = (int16_t)(z * 10);
    s.layer = (uint16_t)(layer < 0 ? 0 : (layer > 65535 ? 65535 : layer));
    if (layer >= 1 && s.z > g_maxz) g_maxz = s.z;
    // Only count the model (layer >= 1) for the view bounds - the purge/prime
    // line printed before the first layer sits at the bed edge and would
    // otherwise shrink the model into a corner.
    if (layer >= 1) {
        if (x1 < g_minf) g_minf = x1; if (x1 > g_maxf_x) g_maxf_x = x1;
        if (x2 < g_minf) g_minf = x2; if (x2 > g_maxf_x) g_maxf_x = x2;
        if (y1 < g_minf_y) g_minf_y = y1; if (y1 > g_maxf_y) g_maxf_y = y1;
        if (y2 < g_minf_y) g_minf_y = y2; if (y2 > g_maxf_y) g_maxf_y = y2;
    }
}

static void parse_gcode(const char* g, uint32_t len) {
    g_seg_count = 0; g_max_layer = 0; g_maxz = 0;
    g_filament_g = scan_filament_g(g, len);
    float px = 0, py = 0, pz = 0; bool have_prev = false;
    int layer = 0;
    g_minf = 1e9f; g_maxf_x = -1e9f; g_minf_y = 1e9f; g_maxf_y = -1e9f;

    const char* c = g;
    const char* gend = g + len;
    while (c < gend) {
        const char* nl = (const char*)memchr(c, '\n', gend - c);
        const char* le = nl ? nl : gend;

        if (c[0] == ';') {
            if ((size_t)(le - c) >= 9 &&
                (memmem(c, le - c, "LAYER_CHANGE", 12) || memmem(c, le - c, "layer num", 9)))
                layer++;
        } else if (c[0] == 'G' && (c[2] == ' ' || c[2] == '\t')) {
            if (c[1] == '0' || c[1] == '1') {                 // linear move
                float x = px, y = py, z = pz, e = 0;
                bool hx = tok(c + 2, le, 'X', &x);
                bool hy = tok(c + 2, le, 'Y', &y);
                bool hz = tok(c + 2, le, 'Z', &z);
                bool he = tok(c + 2, le, 'E', &e);
                if (he && e > 0.0f && (hx || hy) && have_prev) add_seg(px, py, x, y, pz, layer);
                if (hx) px = x;
                if (hy) py = y;
                if (hz) pz = z;
                have_prev = true;
            } else if (c[1] == '2' || c[1] == '3') {          // arc move (flatten)
                float x = px, y = py, i = 0, j = 0, e = 0;
                bool hx = tok(c + 2, le, 'X', &x);
                bool hy = tok(c + 2, le, 'Y', &y);
                bool hi = tok(c + 2, le, 'I', &i);
                bool hj = tok(c + 2, le, 'J', &j);
                bool he = tok(c + 2, le, 'E', &e);
                if (he && e > 0.0f && (hi || hj) && have_prev) {
                    float cx = px + i, cy = py + j;
                    float r = hypotf(px - cx, py - cy);
                    float a0 = atan2f(py - cy, px - cx);
                    float a1 = atan2f(y - cy, x - cx);
                    float da = a1 - a0;
                    if (c[1] == '3') { while (da <= 0) da += 6.2831853f; }   // CCW
                    else             { while (da >= 0) da -= 6.2831853f; }   // CW
                    int steps = (int)(fabsf(da) * r);          // ~1 mm chords
                    if (steps < 1) steps = 1;
                    if (steps > 128) steps = 128;
                    float lx = px, ly = py;
                    for (int s = 1; s <= steps; s++) {
                        float a = a0 + da * (float)s / steps;
                        float nx = cx + r * cosf(a), ny = cy + r * sinf(a);
                        add_seg(lx, ly, nx, ny, pz, layer);
                        lx = nx; ly = ny;
                    }
                }
                if (hx) px = x;
                if (hy) py = y;
                have_prev = true;
            }
        }
        if (!nl) break;
        c = nl + 1;
    }

    g_max_layer = layer;

    // Robust view bounds: 1st..99th percentile of the model endpoints, so stray
    // features (wipe line, a lone wall) don't shrink the model into a corner.
    if (g_maxf_x >= g_minf) {   // fall back to raw bounds
        g_minx = (int16_t)(g_minf * 10);  g_miny = (int16_t)(g_minf_y * 10);
        g_maxx = (int16_t)(g_maxf_x * 10); g_maxy = (int16_t)(g_maxf_y * 10);
    }
    if (g_seg_count > 50) {
        int n = g_seg_count;
        int16_t* xs = (int16_t*)malloc((size_t)n * 2 * sizeof(int16_t));
        int16_t* ys = (int16_t*)malloc((size_t)n * 2 * sizeof(int16_t));
        if (xs && ys) {
            int m = 0;
            for (int i = 0; i < n; i++) {
                if (g_segs[i].layer < 1) continue;      // skip prime line
                xs[m] = g_segs[i].x1; ys[m] = g_segs[i].y1; m++;
                xs[m] = g_segs[i].x2; ys[m] = g_segs[i].y2; m++;
            }
            if (m > 20) {
                qsort(xs, m, sizeof(int16_t), cmp_i16);
                qsort(ys, m, sizeof(int16_t), cmp_i16);
                int lo = m / 100, hi = m - m / 100 - 1;   // 1%..99%
                g_minx = xs[lo]; g_maxx = xs[hi];
                g_miny = ys[lo]; g_maxy = ys[hi];
            }
        }
        free(xs); free(ys);
    }

    Serial.printf("GCODE: %u bytes, %d segs, %d layers, filament=%.1fg, bounds x[%d..%d] y[%d..%d]\n",
                  (unsigned)len, g_seg_count, g_max_layer, g_filament_g, g_minx, g_maxx, g_miny, g_maxy);
}

void gcode_view_load(const char* gcode_file_name) {
    g_ready = false;
    if (!gcode_file_name || !gcode_file_name[0]) return;

    // reset buffers
    g_zip_len = 0;
    if (!g_segs) g_segs = (GcodeSeg*)malloc(sizeof(GcodeSeg) * GV_MAX_SEGS);
    if (!g_segs) { Serial.println("GCODE: seg alloc failed"); return; }

    String path = String("/cache/") + gcode_file_name;
    uint32_t total = 0; String err;
    Serial.printf("GCODE: downloading %s\n", path.c_str());
    bool ok = bambu_ftp_download(path.c_str(), zip_dl_cb, nullptr, &total, &err);
    if (ok) {
        Serial.printf("GCODE: got %u bytes (.3mf)\n", (unsigned)g_zip_len);
        uint32_t glen = 0;
        char* gc = unzip_gcode(&glen);
        if (gc) {
            parse_gcode(gc, glen);
            free(gc);
            g_ready = (g_seg_count > 0);
        } else {
            Serial.println("GCODE: unzip failed (missing plate_1.gcode or out of memory)");
        }
    } else {
        Serial.printf("GCODE: download failed: %s\n", err.c_str());
    }

    // Always release the .3mf ZIP buffer - even on a failed download/unzip - so a
    // failed (and now retried) load never leaks the multi-MB buffer and starves
    // the next attempt. On success this is the same cleanup as before.
    free(g_zip); g_zip = nullptr; g_zip_cap = 0; g_zip_len = 0;
}

bool gcode_view_ready() { return g_ready; }
const GcodeSeg* gcode_view_segments() { return g_ready ? g_segs : nullptr; }
int gcode_view_seg_count() { return g_seg_count; }
int gcode_view_max_layer() { return g_max_layer; }
float gcode_view_filament_g() { return g_filament_g; }
int16_t gcode_view_max_z() { return g_maxz; }
void gcode_view_bounds(int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy) {
    *minx = g_minx; *miny = g_miny; *maxx = g_maxx; *maxy = g_maxy;
}
