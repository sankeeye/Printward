// Android-only: fetches the current print's .3mf over FTP, extracts the sliced
// G-code (Metadata/plate_1.gcode) from the ZIP, parses the top-down XY tool
// paths per layer, and exposes them for the screensaver's live build-up view.
#pragma once
#include <cstdint>

// One extruding move, coordinates in 0.1 mm units (fits a 256 mm bed in int16).
struct GcodeSeg {
    int16_t x1, y1, x2, y2;
    uint16_t layer;
};

// Kick off a load for the given file name (e.g. "foo.stl.3mf"); blocking, does
// the FTP download + unzip + parse. Safe to call again with a new file.
void gcode_view_load(const char* gcode_file_name);

bool gcode_view_ready();                 // parse finished, segments available
const GcodeSeg* gcode_view_segments();   // segment array (or nullptr)
int gcode_view_seg_count();
int gcode_view_max_layer();
// Model XY bounds (0.1 mm units) for scaling to the canvas.
void gcode_view_bounds(int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy);
