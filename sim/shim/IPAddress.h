// Minimal Arduino IPAddress shim. Split out of WiFi.h so PubSubClient.h (which
// includes "IPAddress.h" directly) and WiFi.h share one definition.
#pragma once
#ifndef SIM_IPADDRESS_H
#define SIM_IPADDRESS_H

#include <Arduino.h>
#include <cstdio>

class IPAddress {
public:
    uint8_t o[4] = {0, 0, 0, 0};

    IPAddress() {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { o[0] = a; o[1] = b; o[2] = c; o[3] = d; }

    uint8_t operator[](int i) const { return (i >= 0 && i < 4) ? o[i] : 0; }
    String toString() const {
        char b[16];
        snprintf(b, sizeof(b), "%u.%u.%u.%u", o[0], o[1], o[2], o[3]);
        return String(b);
    }
};

#endif
