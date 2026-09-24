#!/usr/bin/env python
# INCR1 -- the .lodj writer must tell the layout census it wrote a file.
#
# lodgen_layout.sh leg (f) counts what is on disk under the FO4CS root and
# compares it with the census's own "layout <root>, N file(s)" number.  The
# per-chunk cache lands under FO4CSLOD/<ws>/ like everything else, so every
# .lodj written without lodgenNoteLayoutFile() makes the census undercount by
# one.  That is my red, not a neighbour's.
#
#   python p8_lodj_layout_census.py <src/nativeemit.cpp> [--check]
#
# lodgenlayout.h is already included (line 12), so this is the one call.
import io
import sys

LF = chr(10)
TAB = chr(9)

ANCHOR = (TAB + "f.close();" + LF
          + TAB + "g_cacheLastPlacements = ps.size();" + LF)
NEW = (TAB + "f.close();" + LF
       + TAB + "lodgenNoteLayoutFile( path );" + LF
       + TAB + "g_cacheLastPlacements = ps.size();" + LF)


def main():
    p = sys.argv[1]
    raw = io.open(p, "rb").read()
    assert raw.count(chr(13).encode()) == 0, "file has CR, refusing"
    t = raw.decode("utf-8")
    assert '#include "lodgenlayout.h"' in t, "lodgenlayout.h not included"
    if "lodgenNoteLayoutFile( path );" in t:
        sys.stdout.write("already patched" + LF)
        return 0
    n = t.count(ANCHOR)
    assert n == 1, "anchor count == %d, want 1" % n
    out = t.replace(ANCHOR, NEW)
    assert out.count("lodgenNoteLayoutFile( path );") == 1
    if "--check" in sys.argv:
        sys.stdout.write("anchor ok, would grow by %d bytes%s"
                         % (len(out.encode("utf-8")) - len(raw), LF))
        return 0
    io.open(p, "wb").write(out.encode("utf-8"))
    b = io.open(p, "rb").read()
    sys.stdout.write("wrote %d bytes, CR %d, LF %d%s"
                     % (len(b), b.count(chr(13).encode()),
                        b.count(LF.encode()), LF))
    return 0


if __name__ == "__main__":
    sys.exit(main())
