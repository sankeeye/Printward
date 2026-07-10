// Android-only: an on-screen "Printer setup" form (IP / serial / access code)
// so the tablet can be configured without pushing a file over adb. Not built
// for the ESP32/PC-sim (they use the webserver / mock data).
#pragma once

void create_tablet_setup_ui();

// Drain the deferred "save" request. Call once per main-loop iteration; the
// actual file write + MQTT reconnect happen here (not in the click callback)
// so a slow write can't tear a frame mid-redraw.
void tablet_setup_loop();
