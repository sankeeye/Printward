# Inventory of every translatable string in the TABLET UI, before touching it.
#
# Arno's idea, and a good one: get the whole list up front instead of translating
# file by file and discovering things at the end. It immediately paid for itself -
# it showed the German umlauts would all have been tofu boxes (fixed by shipping
# our own font, see tools/gen_fonts.py).
#
# It matches each Dutch string against the Dutch column of the built-in table, so
# strings the web already has a key for come back as "reuse" instead of new work.
#
#     python tools/tablet_inventory.py     -> tablet_inventory.txt
#
# The txt is committed as the work list. Re-run it as you go; "nieuw nodig" should
# drop towards zero.

import io, os, re, glob, json, unicodedata

ROOT = "X:/projects/panda/FilaTrack/"

# --- what keys already exist, and what Dutch text they hold -----------------
lang = io.open(ROOT + "sim/android/lang.cpp", encoding="utf-8", errors="surrogateescape").read()
tbl = lang.split("BUILTIN[]")[1].split("\n};")[0]
rows = re.findall(r'\{"([\w.]+)",\s*"((?:[^"\\]|\\.)*)"(?:\s*"(?:[^"\\]|\\.)*")*\s*,\s*"((?:[^"\\]|\\.)*)"',
                  tbl, re.S)
by_nl = {}
for k, en, nl in rows:
    by_nl.setdefault(nl.strip().lower(), k)
print("existing keys: %d" % len(rows))


def norm(s):
    return s.strip().lower()


# --- every user-visible literal in the tablet UI ----------------------------
SINK = re.compile(r"lv_label_set_text|lv_label_set_text_fmt|lv_msgbox_create|lv_dropdown_set_options|"
                  r"lv_btnmatrix_set_map|lv_textarea_set_placeholder_text|set_hint|lv_table_set_cell_value|"
                  r"lv_checkbox_set_text|lv_roller_set_options|lv_tabview_add_tab")
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
WORD = re.compile(r"[A-Za-zÀ-ſ]{3,}")

items = []
for f in sorted(glob.glob(ROOT + "src/ui_*.cpp") + glob.glob(ROOT + "sim/android/ui_*.cpp")):
    s = io.open(f, encoding="utf-8", errors="surrogateescape").read()
    for i, line in enumerate(s.split("\n"), 1):
        if not SINK.search(line) or 'T("' in line:
            continue
        for m in LIT.finditer(line):
            v = m.group(1)
            if v.startswith("%") or not WORD.search(v):
                continue
            items.append({"file": os.path.basename(f), "line": i, "text": v})

# --- classify ---------------------------------------------------------------
seen, dupes = {}, 0
for it in items:
    n = norm(it["text"])
    it["key"] = by_nl.get(n)                       # reuse an existing key?
    it["status"] = "reuse" if it["key"] else "NEW"
    if n in seen:
        it["dupe_of"] = seen[n]
        dupes += 1
    else:
        seen[n] = "%s:%d" % (it["file"], it["line"])
    # LVGL montserrat is ASCII-only: anything else is a tofu box on the tablet
    bad = sorted({c for c in it["text"] if ord(c) > 126})
    it["nonascii"] = "".join(bad)

reuse = [i for i in items if i["status"] == "reuse"]
new = [i for i in items if i["status"] == "NEW"]
uniq_new = {norm(i["text"]) for i in new}
tofu = [i for i in items if i["nonascii"]]

out = ["=== TABLET UI - INVENTARIS ===", ""]
out.append("totaal teksten      : %d" % len(items))
out.append("  al een sleutel    : %d  (hergebruiken, web deelt ze)" % len(reuse))
out.append("  nieuw nodig       : %d  (%d unieke)" % (len(new), len(uniq_new)))
out.append("  dubbel in de code : %d" % dupes)
out.append("  met niet-ASCII    : %d  <- wordt een blokje op de tablet" % len(tofu))
out.append("")
out.append("--- NIET-ASCII (de LVGL-valkuil) ---")
for i in tofu:
    out.append("  %-22s:%-4d %-40s  tekens: %s" % (i["file"], i["line"], i["text"][:40], i["nonascii"]))
out.append("")
out.append("--- HERGEBRUIK (tekst matcht een bestaande sleutel) ---")
for i in reuse:
    out.append("  %-22s:%-4d %-34s -> %s" % (i["file"], i["line"], i["text"][:34], i["key"]))
out.append("")
out.append("--- NIEUW (sleutel verzinnen) ---")
for i in new:
    d = ("  [dubbel van %s]" % i["dupe_of"]) if "dupe_of" in i else ""
    out.append("  %-22s:%-4d %s%s" % (i["file"], i["line"], i["text"][:52], d))

io.open(ROOT + "tablet_inventory.txt", "w", encoding="ascii", errors="replace").write("\n".join(out))
io.open("C:/Users/w/AppData/Local/Temp/claude/i--Mijn-Drive-Claud-Projects-panda-PandaTouch-streamDeck-src/"
        "c08c6de5-fa97-4704-ad88-5dbfe6a74520/scratchpad/inventory.json", "w",
        encoding="utf-8").write(json.dumps(items, ensure_ascii=False, indent=1))
print("\n".join(out[:12]))
