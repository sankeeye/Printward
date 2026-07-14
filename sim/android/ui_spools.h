#pragma once
#ifndef UI_SPOOLS_H
#define UI_SPOOLS_H

// The "Spools" screen (tablet only): a list of the spool library (fill it in
// via the web), each row with a slot picker + "Laad" that loads that roll into
// an AMS slot (weight tracking + printer AMS over MQTT).
void create_spools_ui();

// The print history / cost-per-print screen (reachable from the Spools screen).
void create_history_ui();

#endif
