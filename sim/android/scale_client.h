#pragma once
#ifndef SCALE_CLIENT_H
#define SCALE_CLIENT_H

// HTTP client for the Printward Scale (the load-cell scale). Runs a background thread
// that polls GET /weight and /info from the scale and executes queued commands
// (tare, calibrate, WiFi/IP change) - all off the LVGL/main thread so the UI
// never blocks on the network. The Scale screen reads the cached values.

extern char g_scale_ip[40];   // scale host/IP (from printward.conf, scale_ip=)

void  scale_client_start();            // start the background poller (once)
void  scale_set_polling(bool on);      // poll only while the Scale screen is open
float scale_grams();                   // last measured weight (g)
bool  scale_stable();                  // last "stable" flag
bool  scale_online();                  // got a reply within the last few seconds
void  scale_info(char* out, int n);    // last /info JSON, or "" if none yet
void  scale_cmd(const char* path);     // queue a GET, e.g. "/tare" or "/cal?known=500"
void  scale_last_msg(char* out, int n);// last command's reply text

#endif
