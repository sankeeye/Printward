#pragma once
#ifndef UI_WIFI_H
#define UI_WIFI_H

// Builds (or rebuilds) the on-device WiFi setup screen and switches to it.
// Lets the user scan for nearby networks, tap one, type a password with the
// on-screen keyboard, and connect - so WiFi can be (re)configured without
// needing the web dashboard (which of course isn't reachable until WiFi
// already works).
void create_wifi_ui();

// Call every loop() iteration. No-op unless a scan was started from this
// screen and is still in progress; non-blocking (just polls WiFi.scanComplete()).
void wifi_ui_loop();

#endif
