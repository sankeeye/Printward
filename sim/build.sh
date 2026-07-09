#!/bin/bash
# Builds the PandaTouch UI simulator (native, SDL) with the MSYS2 UCRT64 gcc.
#
# It compiles the REAL UI sources from ../src (ui_printer/files/settings/
# filament/wifi) plus sim/mocks.cpp and sim/main.cpp, against the sim/shim
# headers (Arduino, WiFi, GFX, esp_lcd... stubs). LVGL is compiled once into
# build/liblvgl.a and cached, so after the first run only the sim/UI sources
# recompile - iterating on the UI is a few seconds.
#
# Run via:  C:\msys64\usr\bin\bash.exe "<path>/sim/build.sh"
# Override the entry file with:  SIM_MAIN=main_poc.cpp bash build.sh
#
# Flags are kept in bash arrays because the project path contains spaces.
set -e

# Ensure MSYS2 tools + gcc are on PATH even when bash is launched non-login.
export PATH="/usr/bin:/ucrt64/bin:$PATH"

UCRT=/ucrt64
GCC="$UCRT/bin/gcc"
GPP="$UCRT/bin/g++"
AR="$UCRT/bin/ar"

SIM="$(cd "$(dirname "$0")" && pwd)"
SRC="$SIM/../src"
LVGL="$SIM/../.pio/libdeps/pandatouch/lvgl"
BUILD="$SIM/build"

if [ ! -d "$LVGL" ]; then
  echo "!! LVGL not found at $LVGL - run 'pio pkg install' in the project first."
  exit 1
fi

# Shim headers must be found before anything else so <Arduino.h> etc. resolve
# to the stubs; ../src is next so the real UI/mqtt/storage headers are used.
CFLAGS=(-I"$SIM/shim" -I"$SIM" -I"$SRC" -I"$LVGL" -I"$UCRT/include" -I"$UCRT/include/SDL2"
        -DLV_CONF_INCLUDE_SIMPLE -O2 -w)

mkdir -p "$BUILD/lvgl"

if [ ! -f "$BUILD/liblvgl.a" ]; then
  echo ">> Compiling LVGL (one-time, ~1-2 min)..."
  i=0
  while IFS= read -r f; do
    "$GCC" "${CFLAGS[@]}" -c "$f" -o "$BUILD/lvgl/o$i.o"
    i=$((i + 1))
  done < <(find "$LVGL/src" -name '*.c')
  echo ">> Archiving $i objects -> liblvgl.a"
  ( cd "$BUILD/lvgl" && "$AR" rcs "$BUILD/liblvgl.a" *.o )
  echo ">> liblvgl.a done."
fi

MAIN="${SIM_MAIN:-main.cpp}"
UI_SOURCES=(
  "$SRC/ui_printer.cpp"
  "$SRC/ui_files.cpp"
  "$SRC/ui_settings.cpp"
  "$SRC/ui_filament.cpp"
  "$SRC/ui_wifi.cpp"
)

echo ">> Building simulator ($MAIN + UI sources)..."
"$GPP" "${CFLAGS[@]}" -std=c++17 \
  "$SIM/$MAIN" "$SIM/mocks.cpp" "${UI_SOURCES[@]}" \
  "$BUILD/liblvgl.a" -L"$UCRT/lib" -lSDL2 -o "$SIM/pandatouch_sim.exe"

# Bundle runtime DLLs so the .exe runs standalone.
for dll in SDL2.dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
  cp -f "$UCRT/bin/$dll" "$SIM/" 2>/dev/null || true
done

echo ">> Built: $SIM/pandatouch_sim.exe"
