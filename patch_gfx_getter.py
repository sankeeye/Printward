Import("env")
import os

# ---------------------------------------------------------------------------
# Adds a public getPanelHandle() accessor to the GFX library's
# Arduino_ESP32RGBPanel class. GFX keeps the esp_lcd panel handle private with
# no getter, but we need it to register a VSYNC callback that calls
# esp_lcd_rgb_panel_restart() every frame - the fix for the ESP32-S3 RGB
# "drift" (the image shifts down permanently after the flash cache is briefly
# disabled by a flash write, or the PSRAM bus stalls under WiFi load).
# See the on_vsync registration in src/pt/pt_display.h.
#
# Kept as a build-time patch (rather than vendoring the whole GFX driver) to
# match this project's existing "massage GFX at build time" approach in
# build_files_exclude.py. Idempotent: a second run detects the marker and
# skips.
# ---------------------------------------------------------------------------

MARKER = "getPanelHandle"
ANCHOR = "uint16_t *getFrameBuffer(int16_t w, int16_t h);"
GETTER = (
    "\n"
    "  // --- FilaTrack patch (patch_gfx_getter.py): expose the private\n"
    "  // esp_lcd panel handle so we can register a VSYNC restart callback\n"
    "  // against the RGB drift. See src/pt/pt_display.h. ---\n"
    "  esp_lcd_panel_handle_t getPanelHandle() { return _panel_handle; }\n"
)

header = os.path.join(
    env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"),
    "GFX Library for Arduino", "src", "databus", "Arduino_ESP32RGBPanel.h")

if not os.path.isfile(header):
    print("** GFX getter patch: header not found, skipping:", header)
elif MARKER in open(header, "r", encoding="utf-8").read():
    print("** GFX getter patch: already applied")
else:
    src = open(header, "r", encoding="utf-8").read()
    if ANCHOR not in src:
        print("** GFX getter patch: anchor not found, skipping (lib version changed?)")
    else:
        src = src.replace(ANCHOR, ANCHOR + GETTER, 1)
        with open(header, "w", encoding="utf-8") as f:
            f.write(src)
        print("** GFX getter patch: applied to", header)
