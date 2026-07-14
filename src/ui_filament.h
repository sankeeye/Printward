#pragma once
#ifndef UI_FILAMENT_H
#define UI_FILAMENT_H

#ifdef __ANDROID__
// One-time: build the roll-picker modal on lv_layer_top() (screen-independent),
// so it works over the Dashboard after the Filament tab was removed. Call once
// at startup, after the display/LVGL is up.
void filament_roll_picker_init();

// Open the roll picker for an AMS/external slot (u*AMS_MAX_TRAYS + t, or 254 for
// the external spool). Tapping a Dashboard AMS slot calls this.
void filament_open_roll_picker(int slot);
#endif

#endif
