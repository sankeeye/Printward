// esp_heap_caps shim (simulator): route capability allocations to plain
// malloc/calloc/free. The capability flags are ignored on PC.
#pragma once
#ifndef SIM_ESP_HEAP_CAPS_H
#define SIM_ESP_HEAP_CAPS_H

#include <cstdint>
#include <cstdlib>

#define MALLOC_CAP_8BIT   0x0004
#define MALLOC_CAP_SPIRAM 0x0400
#define MALLOC_CAP_DMA    0x0008
#define MALLOC_CAP_INTERNAL 0x0800

inline void *heap_caps_malloc(size_t sz, uint32_t) { return malloc(sz); }
inline void *heap_caps_calloc(size_t n, size_t sz, uint32_t) { return calloc(n, sz); }
inline void heap_caps_free(void *p) { free(p); }

#endif
