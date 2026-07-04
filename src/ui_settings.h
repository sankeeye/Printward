#pragma once
#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

// Builds (or rebuilds, to pick up fresh values) the Settings screen and
// switches to it. Call create_printer_ui()'s screen switch (lv_scr_load) via
// the Back button on that screen to return.
void create_settings_ui();

#endif
