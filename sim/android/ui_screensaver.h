// Android-only idle screensaver: a full-screen print dashboard (clock, print
// name, %, layers, remaining time, temps, active AMS slot) that appears after a
// period of no touch and disappears on the next tap.
#pragma once

void create_screensaver();   // build the (hidden) overlay once at startup
void screensaver_loop();     // call every main-loop iteration
