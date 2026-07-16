// .3mf embedded-thumbnail extraction: download a .3mf over FTP and pull out its
// Metadata/plate_1.png (Bambu Studio bakes a model preview into every plate).
// Used by the web Files/Historie previews and the ntfy print-done image.
#include "thumb.h"
#include "bambu_ftp.h"
#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <zlib.h>

static uint32_t z_rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t z_rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

struct ThumbDl { uint8_t* p; uint32_t len, cap; };
static bool thumb_dl_cb(const uint8_t* d, unsigned int n, void* ctx) {
    ThumbDl* b = (ThumbDl*)ctx;
    if (b->len + n > b->cap) {
        uint32_t c = b->cap ? b->cap : 65536;
        while (c < b->len + n) c *= 2;
        uint8_t* nb = (uint8_t*)realloc(b->p, c);
        if (!nb) return false;
        b->p = nb; b->cap = c;
    }
    memcpy(b->p + b->len, d, n); b->len += n; return true;
}

uint8_t* threemf_thumb(const char* path, uint32_t* out_len, int plate) {
    *out_len = 0;
    if (plate < 1) plate = 1;
    ThumbDl zip = {nullptr, 0, 0};
    uint32_t total = 0; String err;
    if (!bambu_ftp_download(path, thumb_dl_cb, &zip, &total, &err) || !zip.p || zip.len < 22) { free(zip.p); return nullptr; }
    int32_t eocd = -1, lim = (int32_t)zip.len - 22;
    for (int32_t i = lim; i >= 0 && i > lim - 65536; i--) if (z_rd32(zip.p + i) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) { free(zip.p); return nullptr; }
    uint32_t cd = z_rd32(zip.p + eocd + 16);
    char target[40];
    snprintf(target, sizeof(target), "Metadata/plate_%d.png", plate);
    size_t tlen = strlen(target);
    uint8_t* out = nullptr;
    uint32_t p = cd;
    while (p + 46 <= zip.len && z_rd32(zip.p + p) == 0x02014b50) {
        uint16_t method = z_rd16(zip.p + p + 10);
        uint32_t comp = z_rd32(zip.p + p + 20), uncomp = z_rd32(zip.p + p + 24);
        uint16_t fnlen = z_rd16(zip.p + p + 28), exlen = z_rd16(zip.p + p + 30), cmlen = z_rd16(zip.p + p + 32);
        uint32_t lho = z_rd32(zip.p + p + 42);
        const char* fn = (const char*)(zip.p + p + 46);
        if (fnlen == tlen && memcmp(fn, target, tlen) == 0 && lho + 30 <= zip.len) {
            uint16_t lfn = z_rd16(zip.p + lho + 26), lex = z_rd16(zip.p + lho + 28);
            uint32_t data = lho + 30 + lfn + lex;
            if ((uint64_t)data + comp <= zip.len) {
                if (method == 0) {
                    out = (uint8_t*)malloc(comp); if (out) { memcpy(out, zip.p + data, comp); *out_len = comp; }
                } else if (method == 8 && uncomp > 0) {
                    out = (uint8_t*)malloc(uncomp);
                    if (out) {
                        z_stream zs; memset(&zs, 0, sizeof(zs));
                        if (inflateInit2(&zs, -15) == Z_OK) {
                            zs.next_in = (Bytef*)(zip.p + data); zs.avail_in = comp;
                            zs.next_out = (Bytef*)out; zs.avail_out = uncomp;
                            int r = inflate(&zs, Z_FINISH); *out_len = (uint32_t)zs.total_out; inflateEnd(&zs);
                            if (r != Z_STREAM_END && r != Z_OK) { free(out); out = nullptr; *out_len = 0; }
                        } else { free(out); out = nullptr; }
                    }
                }
            }
            break;
        }
        p += 46 + fnlen + exlen + cmlen;
    }
    free(zip.p);
    return out;
}
