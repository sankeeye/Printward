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

// Motion actions, shared by the on-screen buttons and the web control page.
enum MoveAction {
    MOVE_XP = 1, MOVE_XM, MOVE_YP, MOVE_YM, MOVE_ZP, MOVE_ZM,
    MOVE_HOME, MOVE_EEXT, MOVE_ERET, MOVE_PREHEAT, MOVE_COOL
};

// Read-only guard check: is this action currently blocked? Sets *reason to a
// short (Dutch) message when it is. Safe to call from any thread (only reads
// the printer status), so the web handler can give immediate feedback.
bool move_blocked(int code, const char** reason);

// Perform a motion action with the given step (mm). Applies move_blocked(),
// sends the g-code via bambu_cmd_gcode() and updates the on-screen hint.
// MUST be called on the main thread (it publishes MQTT). Returns a short
// status string describing what happened.
const char* move_perform(int code, float step);

#endif
