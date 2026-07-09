// Minimal Arduino compatibility shim for the PC simulator.
// Provides just enough of the Arduino core (String, Serial, millis, pin stubs)
// for the PandaTouch UI sources to compile and run natively. NOT a full
// Arduino implementation - only what the UI/display code touches.
#pragma once
#ifndef SIM_ARDUINO_H
#define SIM_ARDUINO_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <chrono>

typedef uint8_t byte;
typedef bool boolean;

// --- pin / digital stubs (never do anything in the sim) ---
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int  digitalRead(int) { return 0; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned long) {}
inline void yield() {}

// --- timing ---
inline unsigned long millis() {
    static auto t0 = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (unsigned long)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
}
inline unsigned long micros() { return millis() * 1000UL; }

// --- ctype helpers (Arduino spellings) ---
inline bool isDigit(char c) { return ::isdigit((unsigned char)c) != 0; }
inline bool isSpace(char c) { return ::isspace((unsigned char)c) != 0; }
inline bool isAlpha(char c) { return ::isalpha((unsigned char)c) != 0; }

// --- Arduino String, backed by std::string ---
class String {
public:
    std::string s;

    String() {}
    String(const char *c) : s(c ? c : "") {}
    String(const std::string &v) : s(v) {}
    String(const String &o) : s(o.s) {}
    String(char c) { s = std::string(1, c); }

    String(int v, int base = 10)            { s = fmtInt((long)v, base); }
    String(unsigned int v, int base = 10)   { s = fmtUInt((unsigned long)v, base); }
    String(long v, int base = 10)           { s = fmtInt(v, base); }
    String(unsigned long v, int base = 10)  { s = fmtUInt(v, base); }
    String(float v, int decimals = 2)       { s = fmtFloat((double)v, decimals); }
    String(double v, int decimals = 2)      { s = fmtFloat(v, decimals); }

    const char *c_str() const { return s.c_str(); }
    unsigned int length() const { return (unsigned int)s.size(); }
    bool isEmpty() const { return s.empty(); }
    char operator[](int i) const { return (i >= 0 && (size_t)i < s.size()) ? s[i] : 0; }

    String &operator=(const char *c) { s = c ? c : ""; return *this; }
    String &operator=(const String &o) { s = o.s; return *this; }

    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *c) { if (c) s += c; return *this; }
    String &operator+=(char c) { s += c; return *this; }

    String operator+(const String &o) const { String r(*this); r.s += o.s; return r; }
    String operator+(const char *c) const { String r(*this); if (c) r.s += c; return r; }

    bool operator==(const String &o) const { return s == o.s; }
    bool operator==(const char *c) const { return c ? (s == c) : false; }
    bool equals(const String &o) const { return s == o.s; }
    bool equals(const char *c) const { return c ? (s == c) : false; }
    bool operator!=(const String &o) const { return s != o.s; }
    bool operator!=(const char *c) const { return !(*this == c); }

    void concat(const char *c, unsigned int len) { if (c) s.append(c, len); }
    void concat(const String &o) { s += o.s; }

    int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(char c, int from) const { auto p = s.find(c, from < 0 ? 0 : from); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(const char *c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(const String &o) const { auto p = s.find(o.s); return p == std::string::npos ? -1 : (int)p; }
    int lastIndexOf(char c) const { auto p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
    int lastIndexOf(char c, int from) const { auto p = s.rfind(c, from < 0 ? 0 : from); return p == std::string::npos ? -1 : (int)p; }

    String substring(int from) const {
        if (from < 0) from = 0;
        if ((size_t)from >= s.size()) return String();
        return String(s.substr(from));
    }
    String substring(int from, int to) const {
        if (from < 0) from = 0;
        if (to < from) to = from;
        if ((size_t)from >= s.size()) return String();
        return String(s.substr(from, to - from));
    }

    long toInt() const { return strtol(s.c_str(), nullptr, 10); }
    float toFloat() const { return strtof(s.c_str(), nullptr); }

    void trim() {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        if (a == std::string::npos) s.clear();
        else s = s.substr(a, b - a + 1);
    }
    void toLowerCase() { for (auto &c : s) c = (char)::tolower((unsigned char)c); }
    void toUpperCase() { for (auto &c : s) c = (char)::toupper((unsigned char)c); }

    bool startsWith(const char *p) const { size_t n = strlen(p); return s.size() >= n && s.compare(0, n, p) == 0; }
    bool startsWith(const String &p) const { return startsWith(p.s.c_str()); }
    bool endsWith(const char *p) const { size_t n = strlen(p); return s.size() >= n && s.compare(s.size() - n, n, p) == 0; }
    bool endsWith(const String &p) const { return endsWith(p.s.c_str()); }

private:
    static std::string fmtInt(long v, int base) {
        char buf[32];
        if (base == 16) snprintf(buf, sizeof(buf), "%lx", v);
        else if (base == 2) { std::string r; unsigned long u = (unsigned long)v; if (!u) return "0"; while (u) { r = char('0' + (u & 1)) + r; u >>= 1; } return r; }
        else snprintf(buf, sizeof(buf), "%ld", v);
        return buf;
    }
    static std::string fmtUInt(unsigned long v, int base) {
        char buf[32];
        if (base == 16) snprintf(buf, sizeof(buf), "%lx", v);
        else snprintf(buf, sizeof(buf), "%lu", v);
        return buf;
    }
    static std::string fmtFloat(double v, int decimals) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.*f", decimals < 0 ? 2 : decimals, v);
        return buf;
    }
};

inline String operator+(const char *c, const String &o) { String r(c); r += o; return r; }

// --- Serial (routed to stdout) ---
class SerialClass {
public:
    void begin(unsigned long) {}
    void print(const char *c) { if (c) fputs(c, stdout); }
    void print(const String &s) { fputs(s.c_str(), stdout); }
    void print(int v) { printf("%d", v); }
    void println() { fputc('\n', stdout); }
    void println(const char *c) { if (c) fputs(c, stdout); fputc('\n', stdout); }
    void println(const String &s) { fputs(s.c_str(), stdout); fputc('\n', stdout); }
    void println(int v) { printf("%d\n", v); }
    template <typename... A> void printf(const char *f, A... a) { ::printf(f, a...); }
    void write(const uint8_t *, unsigned int) {}
    operator bool() const { return true; }
};
extern SerialClass Serial;

#endif // SIM_ARDUINO_H
