// TAMC_GT911 touch shim (simulator). The sim uses SDL mouse input, so the real
// touch read path (pt_touchpad_read) is never called - this only needs to be a
// complete type for pt_display.h to parse.
#pragma once
#ifndef SIM_TAMC_GT911_H
#define SIM_TAMC_GT911_H

#include <cstdint>

struct TAMC_GT911_Point { int x; int y; };

class TAMC_GT911 {
public:
    TAMC_GT911(int = 0, int = 0, int = 0, int = 0, int = 0, int = 0) {}
    bool isTouched = false;
    int touches = 0;
    TAMC_GT911_Point points[5] = {};
    void begin() {}
    void setRotation(int) {}
    void read() {}
};

#endif
