// Minimal Arduino Print base class (Android build). PubSubClient derives from
// Print; WiFiClientSecure (via Client) implements the pure write() methods.
#pragma once
#ifndef SIM_PRINT_H
#define SIM_PRINT_H

#include <Arduino.h>
#include <cstring>

class Print {
public:
    virtual ~Print() {}

    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buf, size_t size) {
        size_t n = 0;
        while (size--) { if (write(*buf++)) n++; else break; }
        return n;
    }

    size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
    size_t print(const char *s) { return write(s); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t println(const char *s) { size_t n = print(s); n += write((uint8_t)'\n'); return n; }
    size_t println() { return write((uint8_t)'\n'); }
};

#endif
