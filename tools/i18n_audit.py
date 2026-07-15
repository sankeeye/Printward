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

# Things the user actually reads.
# `\w+\s*\+?=\s*'<` catches BOTH `h+='<div>'` and the first `var h='<div>'` of a
# builder. Matching only `+=` is what let "— kies —" through: that dropdown is
# built with `o='<option>...'`, an opening assignment, never appended to.
SINK = re.compile(r"\.textContent\s*=|\.innerHTML\s*=|\.placeholder\s*=|\.title\s*=|"
                  r"\balert\(|\bconfirm\(|\bsMsg\(|\bspMsg\(|\btitle=|"
                  r"\b\w+\s*\+?=\s*'<|\bh\s*\+=|\bo\s*\+=")
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

print("=== JS: literals reaching something the user reads ===")
print("\n".join("%4d | %-40s | %s" % h for h in hits))
print("%d hit(s). Prose here belongs in t('key','fallback')." % len(hits))

# --- the static half -------------------------------------------------------
# applyI18n() only rewrites elements carrying data-i18n / -ph / -title. Anything
# else in the markup is frozen in whatever language it was typed in, and no JS
# scan can see it. A title= with no data-i18n-title hid here for three rounds.
from html.parser import HTMLParser


class Static(HTMLParser):
    def __init__(self):
        super().__init__()
        self.depth, self.stack, self.text, self.attrs = 0, [], [], []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        covered = "data-i18n" in a
        if covered:
            self.depth += 1
        self.stack.append(covered)
        for at, key in (("placeholder", "data-i18n-ph"), ("title", "data-i18n-title")):
            if at in a and key not in a and WORD.search(a[at] or ""):
                self.attrs.append((self.getpos()[0], at, a[at][:44]))
        if tag in ("input", "img", "br", "hr", "meta", "link") and self.stack.pop():
            self.depth -= 1

    def handle_endtag(self, tag):
        if self.stack and self.stack.pop():
            self.depth -= 1

    def handle_data(self, d):
        t = d.strip()
        if not self.depth and len(t) > 1 and WORD.search(t):
            self.text.append((self.getpos()[0], t[:52]))


st = Static()
st.feed(src.split("<script>")[0])
print("\n=== HTML: text with no data-i18n ===")
print("\n".join("%4d | %s" % h for h in st.text))
print("%d hit(s). Product names, units and material names are fine here." % len(st.text))
print("\n=== HTML: placeholder/title with no data-i18n-ph / -title ===")
print("\n".join("%4d | %-9s | %s" % h for h in st.attrs) or "  (none)")

shadow = [n for n, l in enumerate(js.split("\n"), 1)
          if re.search(r"function\s*\(\s*t\s*\)|for\s*\(\s*var\s+t\s*=", l)]
if shadow:
    print("\nFAIL: `t` is shadowed on JS line(s) %s - t() will not resolve there."
          % ", ".join(map(str, shadow)))
    sys.exit(1)
if st.attrs:
    print("\nFAIL: the attribute(s) above can never be translated.")
    sys.exit(1)
