"""Lane SSGI1 smoke judge (tests/spells/cell_ssgi.sh): byte-equal and bounce checks on the shot pairs."""
import os
import re
import sys

import numpy as np
from PIL import Image

out = sys.argv[1]


def img(tag):
    p = os.path.join(out, tag + ".png")
    return np.asarray(Image.open(p).convert("RGB")).astype(np.int32) if os.path.exists(p) else None


def notes(tag):
    p = os.path.join(out, tag + ".notes")
    return open(p, errors="replace").read() if os.path.exists(p) else ""


fails = 0


def check(name, ok, detail=""):
    global fails
    print(("  PASS  " if ok else "  FAIL  ") + name + (f"   ({detail})" if detail else ""))
    fails += 0 if ok else 1


base, off, on, on2, redoff = (img(t) for t in ("base", "off", "on", "on2", "redoff"))
for t, a in (("base", base), ("off", off), ("on", on), ("on2", on2), ("redoff", redoff)):
    check(f"{t}.png exists", a is not None)
if fails:
    sys.exit(1)


def diff(a, b):
    d = np.abs(a - b).max(axis=2)
    return int((d > 0).sum()), int(d.max())


n, m = diff(off, base)
check("off == perf1 base, byte for byte", n == 0, f"{n} px differ, max {m}")
n, m = diff(on, off)
lum = lambda a: a.mean()
check("on adds light (mean on > mean off)", lum(on) > lum(off), f"{lum(off):.3f} -> {lum(on):.3f}, {n} px differ, max {m}")
n, m = diff(on, on2)
check("two identical runs: on == on2", n == 0, f"{n} px differ, max {m}")
n, m = diff(redoff, off)
check("red off (computed, not applied) == off", n == 0, f"{n} px differ, max {m}")
txt = notes("on")
check("notes: cell ssgi: on", "cell ssgi: on" in txt)
link = [l for l in txt.splitlines() if re.search(r"(link|compile).*(error|fail)", l, re.I)]
check("notes: no shader compile/link error", not link, "; ".join(link[:3]))
print("VERDICT", "PASS" if fails == 0 else f"FAIL ({fails})")
sys.exit(1 if fails else 0)
