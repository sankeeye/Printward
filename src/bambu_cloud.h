#pragma once
#ifndef BAMBU_CLOUD_H
#define BAMBU_CLOUD_H

#include <Arduino.h>

// Bambu Cloud is *only* used here for one thing: fetching how many grams of
// filament a finished print job actually used (GET /my/tasks), because the
// local/LAN-only MQTT status feed the rest of this firmware relies on never
// exposes that number - it's only ever sent to Bambu's cloud via the slicer.
// Printer monitoring/control (pause/stop/light/etc.) all keep working over
// LAN-only MQTT regardless of whether cloud login is set up.
enum CloudLoginResult {
    CLOUD_LOGIN_OK,
    CLOUD_LOGIN_NEEDS_CODE, // account requires a verification code we can't request from here yet
    CLOUD_LOGIN_FAILED,
};

// Set by bambu_cloud_login() on every non-OK outcome with as much detail as
// we have (HTTP status, Bambu's own error message, or which step failed) -
// surfaced by the web dashboard so "login failed" doesn't stay a total black
// box when debugging.
extern String g_cloud_last_error;

void bambu_cloud_setup();

// Logs in with a Bambu account email+password, saves the resulting access
// token to flash on success (see storage.h). Blocking HTTPS call - only
// meant to be triggered from a web dashboard request handler, never from an
// LVGL click callback (see the tearing-fix comments elsewhere in this repo).
CloudLoginResult bambu_cloud_login(const String& email, const String& password);

bool bambu_cloud_logged_in();
void bambu_cloud_logout();

// Call periodically from the main loop. Internally throttled (polls Bambu
// Cloud every ~60s) and only does anything once logged in and the printer
// serial is known. Non-blocking to the caller in the sense that it only
// fires the (blocking) HTTPS request occasionally, same trade-off as
// bambu_mqtt_loop()'s reconnect attempts.
void bambu_cloud_loop();

// Bambu Cloud's login API sits behind Cloudflare bot-protection that blocks
// the ESP32's TLS fingerprint outright (confirmed: it returns an HTML
// challenge page, not a Bambu error) - so bambu_cloud_login() above will
// basically never succeed from this device. The practical workaround is a
// small helper script on the user's PC (a normal HTTP client isn't blocked)
// that logs into Bambu Cloud itself and reports finished-print weight here
// instead. task_key should be the cloud task's endTime (or any other stable,
// increasing-with-time identifier) so repeated reports of the same print
// don't get double-counted - same dedupe field bambu_cloud_loop() uses
// internally.
void bambu_report_print_weight(float weight_g, const String& task_key);

// Experimental: BTT's own PandaTouch firmware apparently supports Bambu Cloud
// "cloud mode" via an access code, which suggests it may skip the device-side
// /login POST (the part Cloudflare blocks) entirely and instead use a token
// obtained elsewhere. These let the dashboard paste in a token (e.g. one your
// PC relay script already obtained) and test whether an authenticated
// /my/tasks call succeeds from the device even though /login doesn't.
void bambu_cloud_set_token(const String& token);
bool bambu_cloud_poll_now();

#endif
