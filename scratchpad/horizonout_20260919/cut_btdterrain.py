#!/usr/bin/env python3
"""HORIZONOUT: the TERRAIN half of the horizon channel goes with the object half.

`src/io/lodvfile.*` keeps `LODV_ROLE_HORIZON` and every reader-side rule about
it, so a `.lodt` baked by `release/NifSkope.before_horizonout.exe` still opens
in this viewer. What goes is the DRAWING of role 7 as a shadow picture.
"""
import sys

CHECK = "--check" in sys.argv
P = r"E:/Projects/NifskopeWildWastelandEdition/src/btdterrain.cpp"

with open(P, "rb") as f:
    raw = f.read()
cr = raw.count(b"\r")
d = raw.decode("utf-8")
orig = d
N = 0


def sub(old, new, what):
    global d, N
    c = d.count(old)
    if c != 1:
        sys.exit("REFUSED [%s]: anchor appears %d times, want 1" % (what, c))
    d = d.replace(old, new)
    N += 1


def cut(start, end, what, repl=""):
    global d, N
    a = d.find(start)
    if a < 0 or d.find(start, a + 1) >= 0:
        sys.exit("REFUSED [%s]: start anchor not unique" % what)
    b = d.find(end, a)
    if b < 0:
        sys.exit("REFUSED [%s]: end anchor not found after start" % what)
    b += len(end)
    nl = d.find("\n", b)
    b = len(d) if nl < 0 else nl + 1
    d = d[:a] + repl + d[b:]
    N += 1


sub('#include "lodghorizon.h"\n', "", "include lodghorizon.h")

cut("""\t/* WW_LODL_CHANNEL=horizon / horizonbin=<n>, the TERRAIN half (lane HORIZON1,""",
    """\t\t\t\t\t\t.arg( hzWhy.isEmpty() ? QStringLiteral( "no tile covered it" ) : hzWhy );
\t\t\t\t}
\t\t\t}
\t\t}
\t}
""", "terrain horizon channel")

print("btdterrain.cpp %2d edits, %d -> %d chars, %d -> %d lines (CR %d -> %d)"
      % (N, len(orig), len(d), orig.count("\n"), d.count("\n"), cr, d.count("\r")))
if d.count("\r") != cr:
    sys.exit("REFUSED: CR count moved")
if not CHECK:
    with open(P, "wb") as f:
        f.write(d.encode("utf-8"))
    print("OK")
else:
    print("--check: nothing written")
