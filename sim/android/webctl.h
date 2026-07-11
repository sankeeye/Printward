#pragma once
#ifndef WEBCTL_H
#define WEBCTL_H

// Tiny LAN HTTP control server for the Android tablet. Serves a mobile control
// page (the same Move actions as the on-screen tab) and forwards button taps to
// the printer. Requests arrive on a background thread and only enqueue actions;
// the actual MQTT publish happens on the main thread via webctl_loop(), because
// PubSubClient is not thread-safe.

// Start the server thread (call once at setup). No-op if already started.
void webctl_start();

// Drain queued web commands on the MAIN thread (call every loop iteration).
void webctl_loop();

// "http://<lan-ip>:8080" for display, or "" if the address isn't known yet.
const char* webctl_url();

#endif
