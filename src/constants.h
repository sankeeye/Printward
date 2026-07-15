#pragma once
#ifndef PT_CONSTANTS_H
#define PT_CONSTANTS_H

#include <stdint.h>

#define FILATRACK_VERSION "2.0.0-bambu"

// Bambu print state values reported in gcode_state
#define BSTATE_IDLE     "IDLE"
#define BSTATE_RUNNING  "RUNNING"
#define BSTATE_PAUSE    "PAUSE"
#define BSTATE_FINISH   "FINISH"
#define BSTATE_FAILED   "FAILED"
#define BSTATE_PREPARE  "PREPARE"

#endif
