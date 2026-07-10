// Minimal WiFi shim (simulator). ui_wifi.cpp compiles against this; scans
// return empty and status reports "connected" so the WiFi screen renders.
#pragma once
#ifndef SIM_WIFI_H
#define SIM_WIFI_H

#include <Arduino.h>
#include "IPAddress.h"

typedef enum {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_SCAN_COMPLETED = 2,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_CONNECTION_LOST = 5,
    WL_DISCONNECTED = 6
} wl_status_t;

#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED  (-2)
#define WIFI_STA 1

class WiFiClass {
public:
    void mode(int) {}
    int scanNetworks(bool = false) { return 0; }
    int scanComplete() { return 0; }      // 0 networks (sim has no radio)
    void scanDelete() {}
    String SSID(int) { return String("Network"); }
    int32_t RSSI(int) { return -60; }
    void disconnect() {}
    void begin(const char *, const char *) {}
    wl_status_t status() { return WL_CONNECTED; }
    IPAddress localIP() { return IPAddress(192, 168, 2, 33); }
};
extern WiFiClass WiFi;

#endif
