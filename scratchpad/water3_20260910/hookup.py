# -*- coding: utf-8 -*-
"""hookup.py -- lane WATER3's THREE touches to existing source files.

*** NOT APPLIED BY LANE WATER3. *** `Fallout4.exe` was up (pid 12500,
2026-09-10 ~01:5x), so the lane ended BUILD PENDING with this script written and
unrun, per its brief. Run it, then qmake, then make.

    python scratchpad/water3_20260910/hookup.py            # apply
    python scratchpad/water3_20260910/hookup.py --check     # report only

WHY SO FEW TOUCHES. Lane BUILD4 held `src/lodgen.cpp` and `src/nifskope_ui.cpp`
open, so every line of new code went into new files that compile alone. What is
left is registration and dispatch:

  src/nifskope.cpp   one include and one call -- the dock, its Workspaces entry
                     and its self-test all live in watermarkpanel.cpp
  src/nifcli.cpp     the headless verb `lodl <f.lodl> --water-mark-selftest`

Neither file is one lane BUILD4 was compiling. `src/nifskope.cpp` is MIXED and
mostly CRLF, so its anchors carry \\r\\n and the CR count is asserted to rise by
exactly the number of CRLF lines added; `src/nifcli.cpp` is LF-only and its CR
count must stay 0.
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv

# ---------------------------------------------------------------------------
# Every edit: (file, expected CR delta, [(anchor, replacement), ...])
# Anchors carry their own line endings so an anchor that does not match the
# file's real bytes REFUSES instead of writing (ww-contract-provenance).
# ---------------------------------------------------------------------------

NIFSKOPE_CALL = (
    b'\t// Connections (that are required to load after all other inits)\r\n'
    b'\tinitConnections();\r\n'
)
NIFSKOPE_CALL_NEW = (
    b'\t// Connections (that are required to load after all other inits)\r\n'
    b'\tinitConnections();\r\n'
    b'\r\n'
    b'\t/* The Water Marking dock (src/watermarkpanel.cpp): marking water\r\n'
    b'\t * direction by hand on a .lodl, which bungo asked for on 2026-09-09.\r\n'
    b'\t * One call, because the dock, its entry in the Workspaces dropdown, its\r\n'
    b'\t * canvas and its self-test are all built in that file -- the other\r\n'
    b'\t * manager docks are created in nifskope_ui.cpp, which another lane held\r\n'
    b'\t * open when this was written, and this turned out to be the better seam. */\r\n'
    b'\twaterMarkInstall( this );\r\n'
)

EDITS = [
    ('src/nifskope.cpp', 1, [
        (b'#include "qt5compat.hpp"\r\n',
         b'#include "qt5compat.hpp"\r\n#include "watermarkpanel.h"\r\n'),
    ]),
    ('src/nifskope.cpp', 8, [
        (NIFSKOPE_CALL, NIFSKOPE_CALL_NEW),
    ]),
    ('src/nifcli.cpp', 0, [
        (b'#include "lodtfile.h"\n',
         b'#include "lodtfile.h"\n#include "watermark.h"\n'),
        (b'\tbool lodtWaterSelfTestOnly = false;\n',
         b'\tbool lodtWaterSelfTestOnly = false;\n'
         b'\tbool lodtWaterMarkSelfTestOnly = false;\n'),
        (b'\t\telse if ( t == QLatin1String( "--water-selftest" ) ) '
         b'lodtWaterSelfTestOnly = true;\n',
         b'\t\telse if ( t == QLatin1String( "--water-selftest" ) ) '
         b'lodtWaterSelfTestOnly = true;\n'
         b'\t\telse if ( t == QLatin1String( "--water-mark-selftest" ) ) '
         b'lodtWaterMarkSelfTestOnly = true;\n'),
        (b'\t\t  << "  lodl <file.lodl> --water-selftest       the classifier\'s known-answer\\n"\n'
         b'\t\t  << "                                          control, with its refuter\\n"\n',
         b'\t\t  << "  lodl <file.lodl> --water-selftest       the classifier\'s known-answer\\n"\n'
         b'\t\t  << "                                          control, with its refuter\\n"\n'
         b'\t\t  << "  lodl <file.lodl> --water-mark-selftest  the MARKING tool\'s gates. It\\n"\n'
         b'\t\t  << "                                          REWRITES the file it is given,\\n"\n'
         b'\t\t  << "                                          so give it a copy\\n"\n'),
        (b'\telse if ( cmd == QLatin1String( "lodl" ) )\n'
         b'\t\trc = cmdLodt( file, btdInfo, btdHaveRegion,\n'
         b'\t\t\tbtdRegion[0], btdRegion[1], btdRegion[2], btdRegion[3], btdLod,\n'
         b'\t\t\tlodtPlaneName, outFile, lodtWaterCensusOnly, lodtWaterSelfTestOnly );\n',
         b'\telse if ( cmd == QLatin1String( "lodl" ) ) {\n'
         b'\t\t/* --water-mark-selftest runs the MARKING tool\'s own gates\n'
         b'\t\t * (src/watermark.cpp), which rewrite the file, so it is answered\n'
         b'\t\t * here rather than inside the scene builder. */\n'
         b'\t\tif ( lodtWaterMarkSelfTestOnly ) {\n'
         b'\t\t\tQString report, werr;\n'
         b'\t\t\tconst bool ok = lodtWaterMarkSelfTest( file, &report, &werr );\n'
         b'\t\t\tout() << report << Qt::endl;\n'
         b'\t\t\tif ( !ok && !werr.isEmpty() )\n'
         b'\t\t\t\terr() << "error: " << werr << Qt::endl;\n'
         b'\t\t\trc = ok ? 0 : 1;\n'
         b'\t\t} else {\n'
         b'\t\t\trc = cmdLodt( file, btdInfo, btdHaveRegion,\n'
         b'\t\t\t\tbtdRegion[0], btdRegion[1], btdRegion[2], btdRegion[3], btdLod,\n'
         b'\t\t\t\tlodtPlaneName, outFile, lodtWaterCensusOnly, lodtWaterSelfTestOnly );\n'
         b'\t\t}\n'
         b'\t}\n'),
    ]),
]


def main():
    failed = 0
    for rel, crDelta, subs in EDITS:
        path = os.path.join(ROOT, rel)
        b = open(path, 'rb').read()
        cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
        print('%-20s %s %8d bytes %6d lines CR=%d'
              % (rel, hashlib.sha256(b).hexdigest()[:16], n0, lf0, cr0))
        out = b
        for anchor, repl in subs:
            hits = out.count(anchor)
            if hits != 1:
                print('  REFUSED: %d matches for %r' % (hits, anchor[:64]))
                failed += 1
                out = None
                break
            out = out.replace(anchor, repl, 1)
        if out is None:
            continue
        cr1, lf1 = out.count(b'\r'), out.count(b'\n')
        if cr1 - cr0 != crDelta:
            print('  REFUSED: CR moved by %d, expected %d' % (cr1 - cr0, crDelta))
            failed += 1
            continue
        print('  ok: %d -> %d bytes, %d -> %d lines, CR %d -> %d'
              % (n0, len(out), lf0, lf1, cr0, cr1))
        if not check_only:
            open(path, 'wb').write(out)
    if check_only:
        print('--check: nothing written')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
