#pragma once
#ifndef UI_MOVE_H
#define UI_MOVE_H

// Manual motion / control screen (jog X/Y/Z, home, extrude/retract, preheat).
// Mirrors the stock firmware's "Move" tab. All moves are sent as raw g-code
// over MQTT via bambu_cmd_gcode(); jogging is blocked while a print is running.
void create_move_ui();

// Refresh the live nozzle temperature label (call periodically while the screen
// is open). No-op when the Move screen isn't currently built.
void update_move_ui();

#endif
