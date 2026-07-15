import io, glob, re

# lv_font_conv wraps its output in `#if LV_FONT_MONTSERRAT_<n>`, the very switch we
# turn OFF in lv_conf.h to silence LVGL's own copy. Left alone, our font would be
# compiled out too and the UI would come up with no glyphs at all. Swap the guard
# for one of our own that is always on, so the lv_conf.h switch only disables the
# stock font and ours always builds.
for f in sorted(glob.glob("X:/projects/panda/FilaTrack/sim/android/fonts/lv_font_montserrat_*.c")):
    sz = re.search(r"_(\d+)\.c$", f).group(1)
    s = io.open(f, encoding="utf-8", errors="surrogateescape").read()
    old, new = "LV_FONT_MONTSERRAT_%s" % sz, "FILATRACK_FONT_%s" % sz
    if new in s:
        print("  already done:", f.split("/")[-1]); continue
    n = s.count(old)
    s = s.replace(old, new)
    # keep a note in the file itself - this is not obvious to the next reader
    s = s.replace("#ifndef %s" % new,
                  "/* Guard renamed from LV_FONT_MONTSERRAT_%s on purpose: lv_conf.h sets that\n"
                  " * to 0 to drop LVGL's ASCII-only font, and this file must still build. */\n"
                  "#ifndef %s" % (sz, new), 1)
    io.open(f, "w", encoding="utf-8", errors="surrogateescape").write(s)
    print("  %s: %d guard(s) -> %s" % (f.split("/")[-1], n, new))
