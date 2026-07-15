# Rebuild the Montserrat fonts the tablet uses, with Latin-1 added.
#
# Why: LVGL ships Montserrat built with -r 0x20-0x7F,0xB0,0x2022 - ASCII, a degree
# sign and a bullet. Every accented letter is a tofu box, so German (48 of its 274
# strings carry an umlaut), French, Spanish and the Nordic languages are unreadable
# on the device. The web page has no such limit, which is why this only bites on the
# tablet.
#
# Same sizes, same 61 FontAwesome icons, same symbol names as the stock font, so no
# calling code changes. Output goes to sim/android/fonts/ and is committed - you only
# need to run this to change the range or add a size.
#
#     python tools/gen_fonts.py && python tools/fix_font_guards.py
#
# The second script is not optional: lv_font_conv guards its output with
# #if LV_FONT_MONTSERRAT_<n>, which is exactly the switch lv_conf.h sets to 0 to drop
# LVGL's copy - so without it our font compiles to nothing and the UI has no glyphs.
#
# Needs node (npx fetches lv_font_conv). Montserrat + FontAwesome come from LVGL
# itself, under .pio/libdeps/pandatouch/lvgl/scripts/built_in_font/.

import io, os, re, subprocess, sys

LV = "X:/projects/panda/FilaTrack/.pio/libdeps/pandatouch/lvgl"
GEN = LV + "/scripts/built_in_font"
OUT = "X:/projects/panda/FilaTrack/sim/android/fonts"
SIZES = [12, 14, 18, 20, 22, 24, 28, 38, 48]

# Reproduce the stock build exactly, except for the text range. Pull the
# FontAwesome list straight out of a generated file so the icons cannot drift.
src = io.open(LV + "/src/font/lv_font_montserrat_14.c", encoding="utf-8", errors="replace").read()
opts = re.search(r"\* Opts: (.*)", src).group(1)
fa = re.search(r"FontAwesome5-Solid\+Brands\+Regular\.woff -r ([\d,]+)", opts).group(1)
print("FontAwesome glyphs kept:", len(fa.split(",")))

# 0x20-0x7F  ASCII (as before)
# 0xA0-0xFF  Latin-1 supplement: covers the umlauts, accents, and also 0xB0 (deg)
#            and 0xB7 (middle dot), which the stock font only had for 0xB0.
# 2022 bullet (as before), 2026 ellipsis, 20AC euro. (Montserrat has no 2713 check mark - that is
#            why it always showed as a box.)
TEXT_RANGE = "0x20-0x7F,0xA0-0xFF,0x2022,0x2026,0x20AC"

os.makedirs(OUT, exist_ok=True)
for sz in SIZES:
    dst = "%s/lv_font_montserrat_%d.c" % (OUT, sz)
    cmd = ["npx", "--yes", "lv_font_conv",
           "--no-compress", "--no-prefilter", "--bpp", "4", "--size", str(sz),
           "--font", GEN + "/Montserrat-Medium.ttf", "-r", TEXT_RANGE,
           "--font", GEN + "/FontAwesome5-Solid+Brands+Regular.woff", "-r", fa,
           "--format", "lvgl", "-o", dst,
           "--force-fast-kern-format",
           "--lv-font-name", "lv_font_montserrat_%d" % sz,
           "--lv-include", "lvgl.h"]
    r = subprocess.run(cmd, capture_output=True, text=True, shell=True)
    if r.returncode != 0:
        print("FAIL size", sz, r.stderr[-400:])
        sys.exit(1)
    kb = os.path.getsize(dst) / 1024.0
    print("  montserrat_%-2d -> %6.1f KB" % (sz, kb))
print("done")
