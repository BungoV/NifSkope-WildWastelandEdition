#!/usr/bin/env python3
# BUILD2: the GUI half of the .lodt -> .lodl rename that lane RENAME could not
# make (it does not own src/nifskope.cpp / src/nifskope_ui.cpp).
# Source of truth: scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md
#
# Binary splice. src/nifskope.cpp is MIXED and mostly CRLF; every anchor below
# was measured to end CRLF, and every replacement is the same byte length, so
# no line ending can move. src/nifskope_ui.cpp is LF-only.

import sys

ROOT = "E:/Projects/NifskopeWildWastelandEdition/"

EDITS = {
    "src/nifskope.cpp": [
        # 1 - the file-type row (the picker's suffix)
        (b'\t{ "Landscape Terrain", "lodt" },\r\n',
         b'\t{ "Landscape Terrain", "lodl" },\r\n'),
        # comment at ~9674
        (b'\t/* A .lodt is the same species of file and takes the same route: the\r\n',
         b'\t/* A .lodl is the same species of file and takes the same route: the\r\n'),
        # 2 - the load route
        (b'\tif ( file.endsWith( QStringLiteral( ".lodt" ), Qt::CaseInsensitive ) ) {\r\n',
         b'\tif ( file.endsWith( QStringLiteral( ".lodl" ), Qt::CaseInsensitive ) ) {\r\n'),
        # 3 - the suffix branch in load()
        (b'\t} else if ( f.suffix().compare( QLatin1String( "lodt" ), Qt::CaseInsensitive ) == 0 ) {\r\n',
         b'\t} else if ( f.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 ) {\r\n'),
        # the two comments inside "A GENERATED DOCUMENT IS NOT A MODIFIED ONE"
        (b'\t * The .btd and .lodt routes above do not PARSE a document, they BUILD one, and\r\n',
         b'\t * The .btd and .lodl routes above do not PARSE a document, they BUILD one, and\r\n'),
        (b'\t * (NifSkope::save() sends .btd and .lodt to Save As so a game file is never\r\n',
         b'\t * (NifSkope::save() sends .btd and .lodl to Save As so a game file is never\r\n'),
        # 4
        (b'\t\t\t|| f.suffix().compare( QLatin1String( "lodt" ), Qt::CaseInsensitive ) == 0 ) ) {\r\n',
         b'\t\t\t|| f.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 ) ) {\r\n'),
        # 5
        (b'\t\t|| curFile.suffix().compare( QLatin1String( "lodt" ), Qt::CaseInsensitive ) == 0 )\r\n',
         b'\t\t|| curFile.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 )\r\n'),
    ],
    "src/nifskope_ui.cpp": [
        (b'gen->isEnabled() && summary->text().contains( QLatin1String( ".lodt" ) )\n',
         b'gen->isEnabled() && summary->text().contains( QLatin1String( ".lodl" ) )\n'),
    ],
}

rc = 0
for rel, edits in EDITS.items():
    path = ROOT + rel
    with open(path, "rb") as fh:
        b = fh.read()
    cr0, lf0, n0 = b.count(b"\r"), b.count(b"\n"), len(b)
    for old, new in edits:
        c = b.count(old)
        if c != 1:
            print("ABORT %s: anchor count %d for %r" % (rel, c, old[:60]))
            sys.exit(2)
        if len(old) != len(new):
            print("ABORT %s: length change" % rel)
            sys.exit(2)
        b = b.replace(old, new)
    cr1, lf1, n1 = b.count(b"\r"), b.count(b"\n"), len(b)
    if (cr0, lf0, n0) != (cr1, lf1, n1):
        print("ABORT %s: CR %d->%d LF %d->%d bytes %d->%d" % (rel, cr0, cr1, lf0, lf1, n0, n1))
        sys.exit(2)
    with open(path, "wb") as fh:
        fh.write(b)
    print("OK %s: %d edits, CR %d LF %d bytes %d (unchanged)" % (rel, len(edits), cr1, lf1, n1))

# leftovers, for the report
import subprocess
for rel in EDITS:
    out = subprocess.run(["grep", "-c", "lodt", ROOT + rel], capture_output=True, text=True)
    print("remaining 'lodt' in %s: %s" % (rel, out.stdout.strip()))
sys.exit(rc)
