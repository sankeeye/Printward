// Tablet UI scaling. The Android build renders at the tablet's native
// resolution (~1280x767) instead of the 800x480 device design, so remap the
// design fonts to ~1.6x larger ones - text is both bigger AND crisp (no
// upscaling blur). Include this AFTER <lvgl.h> in each UI source; no effect on
// the ESP32 / PC-simulator builds.
#pragma once

#ifdef __ANDROID__
#define lv_font_montserrat_12 lv_font_montserrat_20
#define lv_font_montserrat_14 lv_font_montserrat_22
#define lv_font_montserrat_18 lv_font_montserrat_28
#define lv_font_montserrat_24 lv_font_montserrat_38

// Scale pixel dimensions (row heights, paddings, widget sizes) by ~1.6x so the
// 800x480 design fills the tablet's native ~1280x767 - wrap layout literals in
// PT_SZ(). Identity on the ESP32 / PC-sim.
#define PT_SZ(n) ((int32_t)((n) * 8 / 5))
#else
#define PT_SZ(n) (n)
#endif
