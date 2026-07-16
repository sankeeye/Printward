#pragma once
#ifndef PT_DISPLAY_H
#define PT_DISPLAY_H

// Display glue for the tablet / PC-simulator builds.
//
// This used to be a 427-line header full of ESP32-S3 RGB panel timings, GPIO
// maps and LEDC backlight code for the original handheld device. That target is
// gone - Printward runs on an Android tablet - and the app only ever needed two
// things from it, so the rest went with the firmware it belonged to.

#include <stdint.h>

// Raised by the platform glue (android_glue.cpp / mocks.cpp) while the screen is
// off, so the render loop can skip lv_task_handler().
extern volatile bool pt_display_suspended;

#ifdef __ANDROID__
// The tablet has no hardware backlight PWM, so brightness is a translucent black
// overlay on LVGL's top layer (implemented in sim/main.cpp).
void pt_android_set_brightness(uint8_t percent);

inline void pt_set_backlight(uint8_t percent, bool save)
{
    if (percent > 100) percent = 100;
    pt_android_set_brightness(percent);
    (void)save;   // the level is persisted by save_settings(), not here
}
#else
// PC simulator: there is no backlight to drive.
inline void pt_set_backlight(uint8_t percent, bool save)
{
    (void)percent;
    (void)save;
}
#endif

#endif
