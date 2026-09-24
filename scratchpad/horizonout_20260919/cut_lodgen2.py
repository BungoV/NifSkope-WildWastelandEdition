#!/usr/bin/env python3
"""HORIZONOUT: the two leftovers the first lodgen sweep did not reach.

`rawHorizon` in `lodgenVtEstimateBounds` survived because the block that
defined `hzSheets`/`hzLevel` ended on its own `: 0;` line, and the estimator's
stale `// the horizon sheets` comment in `lodgenVtTileBytes` described a term
that is gone.
"""
import sys

CHECK = "--check" in sys.argv
P = r"E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp"

with open(P, "rb") as f:
    raw = f.read()
cr = raw.count(b"\r")
d = raw.decode("utf-8")
orig = d


def sub(old, new, what):
    global d
    c = d.count(old)
    if c != 1:
        sys.exit("REFUSED [%s]: anchor appears %d times, want 1" % (what, c))
    d = d.replace(old, new)


sub("""\t\tif ( withEmissive )
\t\t\tn += blocks * 8;                        // emissive, BC1, no alpha
\t\t// the horizon sheets, RGBA8 uncompressed, four azimuth bins each
""",
    """\t\tif ( withEmissive )
\t\t\tn += blocks * 8;                        // emissive, BC1, no alpha
""",
    "stale horizon-sheet comment in lodgenVtTileBytes")

sub("""\tconst qint64 rawHorizon = hzSheets
\t\t? ( lodgenVtTileBytes( stored, opts.mips, false, opts.height, false,
\t\t\t\topts.coverInColor, hzSheets )
\t\t\t- lodgenVtTileBytes( stored, opts.mips, false, opts.height, false,
\t\t\t\topts.coverInColor, 0 ) )
\t\t: 0;
""", "", "rawHorizon in lodgenVtEstimateBounds")

print("lodgen.cpp %d -> %d chars, %d -> %d lines (CR %d -> %d)"
      % (len(orig), len(d), orig.count("\n"), d.count("\n"), cr, d.count("\r")))
if d.count("\r") != cr:
    sys.exit("REFUSED: CR count moved")
if not CHECK:
    with open(P, "wb") as f:
        f.write(d.encode("utf-8"))
    print("OK")
else:
    print("--check: nothing written")
