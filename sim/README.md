# FilaTrack UI simulator

Runs the **real** LVGL UI from `../src` (printer, files, settings, filament,
wifi screens) in an 800×480 window **on the PC**, fed with mock data - so you
can iterate on the interface without flashing the device or needing the screen.
Mouse = touch.

## Use it

Double-click **`run_sim.bat`** - it builds and launches the simulator.

Workflow: edit a UI source in `../src` (e.g. `ui_printer.cpp`), double-click
`run_sim.bat` again, and the window shows your change in a few seconds. When
you're happy, flash the device as usual (`pio run -t upload`).

## What is / isn't simulated

- **Is:** every LVGL screen and widget, layout, colours, fonts, and the click
  navigation between screens. Control buttons (pause/stop/light/speed) log to
  the console and flip the local fake state.
- **Isn't:** real WiFi/MQTT/FTP/printer (stubbed with fake data in
  `mocks.cpp`), and the ESP32 RGB-panel timing/drift (hardware only).

Change the fake print status, AMS colours, file list, etc. by editing
`sim_load_mock_data()` and `bambu_ftp_list()` in **`mocks.cpp`**.

## How it works

The real `src/*.cpp` UI files are compiled natively against small **shim
headers** in `shim/` that stand in for the Arduino/ESP hardware headers
(`Arduino.h`, `WiFi.h`, `Arduino_GFX_Library.h`, `driver/ledc.h`,
`esp_lcd_panel_rgb.h`, ...). `mocks.cpp` defines the symbols the UI expects
from the hardware/network modules. LVGL's built-in SDL driver does the drawing.

- `lv_conf.h` - copy of the device config with `LV_USE_SDL=1`, `LV_USE_OS=NONE`,
  and the Arduino-LittleFS FS driver disabled.
- `build.sh` - compiles LVGL once into `build/liblvgl.a` (cached), then the
  sim + UI sources. Run via `run_sim.bat`, or directly in an MSYS2 shell.

## Requirements

MSYS2 with the UCRT64 toolchain + SDL2 (already installed at `C:\msys64`):

```
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 make
```

LVGL is used from `../.pio/libdeps/pandatouch/lvgl` (run `pio pkg install` in
the project if that folder is missing).
