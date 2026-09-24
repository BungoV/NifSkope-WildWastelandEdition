#!/usr/bin/env python3
"""Lane HKX5's hook-up, AS A REFUSING SCRIPT (skill `ww-anchored-hookup`).

Two files have to reach NifSkope.pro before src/gltfimport.cpp and
src/hkxwrite.cpp are part of the application build:

    HEADERS  src/gltfimport.h, src/hkxwrite.h
    SOURCES  src/gltfimport.cpp, src/hkxwrite.cpp

NOTHING IS APPLIED.  The lane brief says hook-ups are delivered as a refusing
script, and NifSkope.pro is a shared file that lanes HKX2b, HKX4b (and the
`.pro` line for their own new sources) are also queued to touch -- one lane per
file, CONSTITUTION rule 1.  This script:

  * finds the ANCHOR TEXT (not a line number) each insertion belongs after,
  * prints the exact edit it WOULD make, and the line it would land on now,
  * verifies the anchors are unique and that the entries are not already there,
  * exits 3 ("refused: nothing applied") in every case.

`--force-i-have-the-file` is deliberately NOT implemented.  Whoever owns
NifSkope.pro next applies these four lines by hand, or a merge lane splices
them with the other lanes' lines in one pass.

usage: python scratchpad/hkx5_20260910/hookup.py [--pro NifSkope.pro]
"""
import os, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))

# (anchor line text, the line to insert AFTER it, what it is)
EDITS = [
    ("\tsrc/hkxanim.h \\",        "\tsrc/gltfimport.h \\",   "HEADERS: the glTF animation importer"),
    ("\tsrc/hkxplayback.h \\",    "\tsrc/hkxwrite.h \\",     "HEADERS: the .hkx animation writer"),
    ("\tsrc/hkxanim.cpp \\",      "\tsrc/gltfimport.cpp \\", "SOURCES: the glTF animation importer"),
    ("\tsrc/hkxplayback.cpp \\",  "\tsrc/hkxwrite.cpp \\",   "SOURCES: the .hkx animation writer"),
]


def main():
    pro = os.path.join(REPO, 'NifSkope.pro')
    if '--pro' in sys.argv:
        pro = sys.argv[sys.argv.index('--pro') + 1]
    if not os.path.exists(pro):
        print("REFUSED: '%s' does not exist" % pro)
        return 3
    raw = open(pro, 'rb').read()
    print("NifSkope.pro: %d bytes, %d lines, %d CR (line endings are measured with byte counts, never grep)"
          % (len(raw), raw.count(b'\n'), raw.count(b'\r')))
    lines = raw.decode('utf-8').split('\n')

    ok = True
    for anchor, ins, what in EDITS:
        hits = [i for i, l in enumerate(lines) if l.rstrip('\r') == anchor]
        already = [i for i, l in enumerate(lines) if l.rstrip('\r') == ins]
        if already:
            print("  ALREADY PRESENT  %-42s at line %d" % (ins.strip(), already[0] + 1))
            continue
        if len(hits) != 1:
            print("  ANCHOR NOT UNIQUE  '%s' occurs %d times -- this hook-up cannot be placed by text"
                  % (anchor.strip(), len(hits)))
            ok = False
            continue
        print("  WOULD INSERT  %-24s after line %-5d (%s)   [%s]"
              % (ins.strip(), hits[0] + 1, anchor.strip(), what))

    print()
    if not ok:
        print("REFUSED: an anchor is missing or ambiguous; nothing applied, and nothing would be safe to apply.")
        return 3
    print("REFUSED BY DESIGN: nothing was written.  NifSkope.pro is shared with the other live HKX lanes;")
    print("apply the four lines above by hand, or let one merge lane splice every lane's .pro lines at once.")
    print("Until then src/gltfimport.cpp and src/hkxwrite.cpp build only through")
    print("scratchpad/hkx5_20260910/build_dump.sh (release/hkxwrite_dump.exe), which is how the 23 gates ran.")
    return 3


if __name__ == '__main__':
    sys.exit(main())
