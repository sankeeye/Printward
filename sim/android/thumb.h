#pragma once
#ifndef THUMB_H
#define THUMB_H

#include <cstdint>

// Download a .3mf over FTP and return its embedded model thumbnail
// (Metadata/plate_1.png) as malloc'd PNG bytes; *out_len set to the length.
// Caller free()s the buffer. Returns nullptr on any failure (no such file,
// no thumbnail, download/inflate error). Shared by the Files/Historie web
// previews and the ntfy print-done notification image. Android-only.
uint8_t* threemf_thumb(const char* ftp_path, uint32_t* out_len);

#endif
