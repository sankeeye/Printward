#pragma once
#ifndef THUMB_H
#define THUMB_H

#include <cstdint>

// Download a .3mf over FTP and return an embedded plate thumbnail
// (Metadata/plate_<plate>.png) as malloc'd PNG bytes; *out_len set to the length.
// `plate` is 1-based (default 1); a multi-plate project has one PNG per plate.
// Caller free()s the buffer. Returns nullptr on any failure (no such file,
// no thumbnail, download/inflate error). Shared by the Files/Historie web
// previews and the ntfy print-done notification image. Android-only.
uint8_t* threemf_thumb(const char* ftp_path, uint32_t* out_len, int plate = 1);

#endif
