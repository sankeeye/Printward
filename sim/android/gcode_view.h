// Android-only: fetches the current print's .3mf over FTP, extracts the sliced
// G-code (Metadata/plate_1.gcode) from the ZIP, parses the top-down XY tool
// paths per layer, and exposes them for the screensaver's live build-up view.
#pragma once
#include <cstdint>

// One extruding move, coordinates in 0.1 mm units (fits a 256 mm bed in int16).
struct GcodeSeg {
    int16_t x1, y1, x2, y2;
    int16_t z;          // layer height (0.1 mm) - for the 3D view
    uint16_t layer;
};

// Max Z (0.1 mm) of the model, for scaling the 3D view.
int16_t gcode_view_max_z();

// Kick off a load for the given file name (e.g. "foo.stl.3mf"); blocking, does
// the FTP download + unzip + parse. Safe to call again with a new file.
void gcode_view_load(const char* gcode_file_name);

bool gcode_view_ready();                 // parse finished, segments available
const GcodeSeg* gcode_view_segments();   // segment array (or nullptr)
int gcode_view_seg_count();
int gcode_view_max_layer();
// Total filament for this print in grams (from the slicer comment), 0 = unknown.
float gcode_view_filament_g();
// Model XY bounds (0.1 mm units) for scaling to the canvas.
void gcode_view_bounds(int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy);

// --- multi-plate projects ----------------------------------------------------
// A Bambu project .3mf may hold several plates; the printer prints one but does
// not report which. gcode_view picks the plate whose layer count matches the
// printer's, unless the user forces one. All are 1-based.
int  gcode_view_active_plate();     // effective plate (manual override, else auto)
int  gcode_view_plate_count();      // plates in the current .3mf (>= 1)
int  gcode_view_manual_plate();     // 0 = auto, else the forced plate
int  gcode_view_loaded_plate();     // plate whose gcode is parsed now (0 = none)
bool gcode_view_detect_deferred();  // true if auto-detect ran before layers known
void gcode_view_set_manual_plate(int plate);  // 0 = auto
