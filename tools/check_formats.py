# Verify a translation's printf specifiers match the English original.
#
# Why this is not optional: several keys are format strings ("Layer %d / %d") that
# go straight into lv_label_set_text_fmt / snprintf. A translator who drops a %d,
# or writes %s where the code passes an int, does not get a wrong label - they get
# a crash or garbage memory read on the device. The language file is meant to be
# edited by people who will never compile this, so the check has to live here.
#
#     python tools/check_formats.py        # exits non-zero if a language is unsafe

import io, os, re, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = re.compile(r"%[-+ #0]*[\d.*]*(?:hh|h|ll|l|L|z|j|t)?([diuoxXeEfgGaAcspn%])")

lang = io.open(os.path.join(ROOT, "sim", "android", "lang.cpp"),
               encoding="utf-8", errors="surrogateescape").read()
tbl = lang.split("BUILTIN[]")[1].split("\n};")[0]
rows = re.findall(r'\{"([\w.]+)",\s*"((?:[^"\\]|\\.)*)"(?:\s*"(?:[^"\\]|\\.)*")*\s*,\s*"((?:[^"\\]|\\.)*)"',
                  tbl, re.S)


def specs(s):
    return [c for c in SPEC.findall(s) if c != "%"]


base = {}
for k, en, nl in rows:
    base[k] = specs(en)
    if specs(nl) != specs(en):
        print("FAIL builtin nl  %-22s en=%s nl=%s" % (k, specs(en), specs(nl)))

bad = 0
for p in glob.glob(os.path.join(ROOT, "lang", "printward_lang_*.conf")):
    code = re.search(r"lang_(\w+)\.conf", p).group(1)
    for i, line in enumerate(io.open(p, encoding="utf-8").read().split("\n"), 1):
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        if k not in base:
            continue
        if specs(v) != base[k]:
            print("FAIL %s:%d  %-22s expected %s, got %s" % (code, i, k, base[k], specs(v)))
            bad += 1

print("checked %d keys against %d language file(s)" % (len(base), len(glob.glob(os.path.join(ROOT, "lang", "*.conf")))))
if bad:
    print("\n%d translation(s) would crash or print garbage. Fix before shipping." % bad)
    sys.exit(1)
print("all format strings line up")
