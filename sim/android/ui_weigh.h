#pragma once
#ifndef UI_WEIGH_H
#define UI_WEIGH_H

// The "Scale" screen (tablet only): live weight from the FilaTrack Scale plus tare,
// calibrate and network management (WiFi change, scale IP), all over HTTP via
// scale_client. Reached from a "Scale" button in the printer header.
void create_weigh_ui();
void update_weigh_ui();   // refresh live weight / info (call periodically)

#endif
