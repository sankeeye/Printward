#pragma once
#ifndef UI_FILAMENT_H
#define UI_FILAMENT_H

// Builds (or rebuilds) the Filament screen, showing each AMS tray's material
// type, color, and estimated remaining amount, plus the external/manual
// spool holder - and switches to it.
void create_filament_ui();

// Call periodically (same cadence as update_printer_ui()) while this screen
// might be visible, to refresh the live remaining-% values. Cheap no-op if
// the screen hasn't been built yet.
void update_filament_ui();

#endif
