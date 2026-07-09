// Arduino_GFX shim (simulator). Stub panel/display classes so pt_display.h's
// inline functions parse. In the sim, LVGL's SDL driver does the real drawing,
// so these methods are never actually called - they only need to exist.
#pragma once
#ifndef SIM_ARDUINO_GFX_H
#define SIM_ARDUINO_GFX_H

#include <cstdint>
#include <esp_lcd_panel_rgb.h>

#ifndef GFX_NOT_DEFINED
#define GFX_NOT_DEFINED (-1)
#endif

class Arduino_ESP32RGBPanel {
public:
    Arduino_ESP32RGBPanel() {}
    esp_lcd_panel_handle_t getPanelHandle() { return nullptr; }
};

class Arduino_RGB_Display {
public:
    Arduino_RGB_Display() {}
    bool begin(int32_t = GFX_NOT_DEFINED) { return true; }
    void fillScreen(uint32_t) {}
    int16_t width() { return 800; }
    int16_t height() { return 480; }
    void draw16bitRGBBitmap(int16_t, int16_t, uint16_t *, int16_t, int16_t) {}
    void setTextColor(uint32_t) {}
    void setTextSize(uint8_t) {}
    void setCursor(int16_t, int16_t) {}
    void print(const char *) {}
};

#endif
