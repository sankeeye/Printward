// Android compatibility shims for old API levels (SM-T280 = API 22).
#include <malloc.h>
#include <stddef.h>

// aligned_alloc() only entered Android's libc at API 28. LVGL's SDL driver
// (texture_resize in lv_sdl_window.c) calls it, so provide it via memalign,
// which has always been available.
#if !defined(__ANDROID_API__) || __ANDROID_API__ < 28
void *aligned_alloc(size_t alignment, size_t size) {
    return memalign(alignment, size);
}
#endif
