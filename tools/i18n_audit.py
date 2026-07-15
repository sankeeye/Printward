# Find text the web page can show that does NOT go through the translator.
#
# Why this exists: "not everything is English" came back three times. Every time,
# the hunt was done by grepping for Dutch words I could think of, so the result was
# only ever as good as my word list - and each round it missed a different corner
# ("materialen", "geselecteerd", "stabiel"). This scans by structure instead: take
# every string literal that reaches something the user reads, and show it. No word
# list, no language assumptions.
#
# It reports, it does not judge. Plenty of hits are fine (element ids, CSS, PLA,
# "mm", Bambu's own Silent/Sport names). You read the list and decide. Anything
# that is prose meant for a human belongs in t('key','fallback') or data-i18n.
#
# The runtime side is covered by selftest.ps1 (every language complete, no empty
# strings). That cannot see hardcoded text, which is exactly this script's job.
#
#     python tools/i18n_audit.py
#
# Watch for these three, they caused real bugs:
#   1. JS that rebuilds an element with fixed text, overwriting its data-i18n
#      (that is how "alle materialen" survived being "translated").
#   2. A local named `t` (a loop var, a callback arg) shadowing the t() function.
#   3. data-i18n on an element that wraps a control - it sets innerHTML and would
#      delete the control.

import io, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGE = os.path.join(ROOT, "sim", "android", "webctl_page.h")

src = io.open(PAGE, encoding="utf-8", errors="surrogateescape").read()
js = src.split("<script>")[1].split("</script>")[0]
head = src.index("<script>")

# things the user actually reads
SINK = re.compile(r"\.textContent\s*=|\.innerHTML\s*=|\.placeholder\s*=|\balert\(|\bconfirm\(|"
                  r"\bh\s*\+=|\bo\s*\+=|\bsMsg\(|\bspMsg\(|\btitle=")
LIT = re.compile(r"'((?:[^'\\\n]|\\.)*)'")
FALLBACK = re.compile(r"t\(\s*'[\w.]+'\s*,\s*$")     # 2nd arg of t() is a default
TAG = re.compile(r"<[^>]*>")
ENT = re.compile(r"&[a-z#0-9]+;")
WORD = re.compile(r"[A-Za-zÀ-ſ]{3,}")

hits = []
for i, line in enumerate(js.split("\n"), 1):
    if not SINK.search(line):
        continue
    for m in LIT.finditer(line):
        v = m.group(1)
        if FALLBACK.search(line[max(0, m.start() - 60):m.start()]):
            continue
        prose = ENT.sub(" ", TAG.sub(" ", v))                   # drop the markup
        prose = re.sub(r"[\s·•|/,:;.\-_=+()\[\]{}#%*0-9]+", " ", prose).strip()
        words = [w for w in prose.split() if WORD.fullmatch(w)]
        if words:
            hits.append((src[:head].count("\n") + i, " ".join(words)[:40], v[:54]))

out = ["%4d | %-40s | %s" % h for h in hits]
print("\n".join(out))
print("\n%d literal(s) reaching a visible sink." % len(hits))

shadow = [n for n, l in enumerate(js.split("\n"), 1)
          if re.search(r"function\s*\(\s*t\s*\)|for\s*\(\s*var\s+t\s*=", l)]
if shadow:
    print("\nWARNING: `t` is shadowed on JS line(s) %s - t() will not resolve there."
          % ", ".join(map(str, shadow)))
    sys.exit(1)
