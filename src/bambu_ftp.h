#pragma once
#ifndef BAMBU_FTP_H
#define BAMBU_FTP_H

#include <Arduino.h>

// Bambu printers expose their SD card (and an internal "cache" folder used
// for files sent directly from the slicer) over FTPS on port 990 - see
// ftp.md in github.com/Doridian/OpenBambuAPI. There's no local-network way
// to list files over MQTT, so this is a small, purpose-built implicit-FTPS
// client (just enough to log in, CWD, and LIST a directory) rather than a
// general FTP library, since nothing suitable exists for Arduino/ESP32.

#define FTP_MAX_ENTRIES 40

struct FtpEntry {
    String name;
    bool is_dir;
    uint32_t size;
};

// Lists the contents of `path` (e.g. "/" for the SD card root, "/cache" for
// recently-sent slicer files). Blocking (a couple of TLS handshakes plus a
// directory transfer) - only meant to be called from the Files screen when
// the user opens/refreshes it, never from a tight loop. Returns true on
// success (even if the directory is empty); false with `err` filled in on
// any failure (connection, login, or protocol error).
bool bambu_ftp_list(const char* path, FtpEntry* out, int max_entries, int* out_count, String* err);

// Streams a file (e.g. "/cache/foo.3mf") down over FTPS, handing each chunk to
// `cb` (return false from it to abort). Blocking. `out_bytes` gets the total
// received. Returns true on success, false with `err` on failure.
typedef bool (*ftp_download_cb)(const uint8_t* data, unsigned int len, void* ctx);
bool bambu_ftp_download(const char* full_path, ftp_download_cb cb, void* ctx,
                        uint32_t* out_bytes, String* err);

// Delete a file on the printer over FTP (DELE). Returns false + err on failure.
bool bambu_ftp_delete(const char* full_path, String* err);

#endif
