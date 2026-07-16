// Android-only idle screensaver: a full-screen print dashboard (clock, print
// name, %, layers, remaining time, temps, active AMS slot) that appears after a
// period of no touch and disappears on the next tap.
#pragma once

void create_screensaver();   // build the (hidden) overlay once at startup
void screensaver_loop();     // call every main-loop iteration

// Screensaver model view: false = top-down 2D, true = isometric 3D. Chosen in
// Settings, persisted in /sdcard/printward.conf.
extern bool g_screensaver_3d;
void screensaver_view_changed();   // call after flipping g_screensaver_3d to redraw
