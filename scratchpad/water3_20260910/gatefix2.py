# -*- coding: utf-8 -*-
"""gatefix2.py -- lane BUILD5b: the two gates still red after gatefix.py.

  P3 undo, 480 bytes differ (was 1,021,405).  The restore added by gatefix.py
     works, but `tableAtOpen` is re-taken by open() -- and save() RE-OPENS the
     file it just wrote, so after the first save the snapshot was the MARKED
     table and "undo" restored the marks.  The residue is body 2's own record
     (flags bit 1, flowX/flowY, confidence) at 0x2249B19 and the flow-plane
     tiles derived from it.  Fixed by carrying the snapshot across save()'s
     re-open: it means "the derived fields as the GENERATOR wrote them", for
     the life of the document, and that is the only thing "with the stroke gone
     there is nothing left to remember" can mean.

  dock dry land, 'that stroke has no points'.  THE CANVAS, not the test: it
     dropped every point that was not on water before the model ever saw the
     stroke, so a stroke drawn entirely on dry land arrived empty and was
     refused for the wrong reason, in a sentence that does not say what went
     wrong.  A user would have got the same.  Fixed in two places: the canvas
     hands over the stroke AS DRAWN, and addStroke names the body from the
     first point that lands on water instead of strictly the first point -- so
     a stroke that starts a few units off the bank is still forgiven, which is
     what the filter was really buying, and only a stroke that touches no water
     anywhere is refused.

    python scratchpad/water3_20260910/gatefix2.py --check
    python scratchpad/water3_20260910/gatefix2.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv

# ---- 1. the snapshot survives save()'s re-open -----------------------------
C_SAVE_ANCHOR = (
    b'\tconst QString keep = filePath;\n'
    b'\tconst int wrote = marks.size();\n'
    b'\tQString err;\n'
    b'\tif ( !open( keep, &err ) )\n'
)
C_SAVE_NEW = (
    b'\tconst QString keep = filePath;\n'
    b'\tconst int wrote = marks.size();\n'
    b'\t/* open() re-takes tableAtOpen, and this is a re-open of a file we have\n'
    b'\t * just marked -- so without carrying the snapshot across, "the table as\n'
    b'\t * the GENERATOR wrote it" would silently become "the table as this\n'
    b'\t * session last saved it", and removing a stroke would restore the stroke.\n'
    b'\t * That is exactly what gate P3 caught, by 480 bytes. */\n'
    b'\tconst QVector<LodtWaterBody> genTable = tableAtOpen;\n'
    b'\tQString err;\n'
    b'\tif ( !open( keep, &err ) )\n'
)

C_SAVE2_ANCHOR = (
    b'\tif ( marks.size() != wrote )\n'
    b'\t\treturn fail( QStringLiteral( "%1 strokes were written and %2 read back" )\n'
    b'\t\t\t.arg( wrote ).arg( marks.size() ) );\n'
)
C_SAVE2_NEW = C_SAVE2_ANCHOR + (
    b'\tif ( genTable.size() == tableAtOpen.size() )\n'
    b'\t\ttableAtOpen = genTable;\n'
)

# ---- 2. the body is named by the first point that lands on water -----------
C_ADD_ANCHOR = (
    b'\tWaterStroke s = in;\n'
    b'\tif ( !s.body )\n'
    b'\t\ts.body = bodyAtWorld( double( s.pts.first().x ), double( s.pts.first().y ) );\n'
    b'\tif ( !s.body ) {\n'
    b'\t\t/* THE CONTROL, and it is a refusal in words on purpose. A constraint\n'
    b'\t\t * that names no body constrains nothing, and storing it would leave the\n'
    b'\t\t * next reader to work out why the planes did not move. */\n'
    b'\t\tsay( QStringLiteral( "that stroke starts on dry land, so it names no body of water; "\n'
    b'\t\t\t"nothing was stored" ) );\n'
    b'\t\treturn false;\n'
    b'\t}\n'
)
C_ADD_NEW = (
    b'\tWaterStroke s = in;\n'
    b'\t/* The FIRST POINT THAT LANDS ON WATER names the body, not strictly the\n'
    b'\t * first point: a drag that begins a few units off the bank is the same\n'
    b'\t * mark the user meant, and the canvas used to buy that forgiveness by\n'
    b'\t * throwing dry points away before this function ever saw them -- which\n'
    b'\t * also turned a stroke drawn entirely on land into "that stroke has no\n'
    b'\t * points", a sentence that says nothing about what went wrong. */\n'
    b'\tif ( !s.body )\n'
    b'\t\tfor ( const WaterStrokePoint & p : s.pts ) {\n'
    b'\t\t\ts.body = bodyAtWorld( double( p.x ), double( p.y ) );\n'
    b'\t\t\tif ( s.body )\n'
    b'\t\t\t\tbreak;\n'
    b'\t\t}\n'
    b'\tif ( !s.body ) {\n'
    b'\t\t/* THE CONTROL, and it is a refusal in words on purpose. A constraint\n'
    b'\t\t * that names no body constrains nothing, and storing it would leave the\n'
    b'\t\t * next reader to work out why the planes did not move. */\n'
    b'\t\tsay( QStringLiteral( "that stroke touches no water anywhere along it -- it is on "\n'
    b'\t\t\t"dry land, so it names no body of water; nothing was stored" ) );\n'
    b'\t\treturn false;\n'
    b'\t}\n'
)

# ---- 3. the canvas hands over the stroke AS DRAWN --------------------------
P_LAY_ANCHOR = (
    b'\tconst int steps = 64;\n'
    b'\tfor ( int k = 0; k <= steps; k++ ) {\n'
    b'\t\tconst double t = double( k ) / steps;\n'
    b'\t\tconst double wx = x0 + ( x1 - x0 ) * t;\n'
    b'\t\tconst double wy = y0 + ( y1 - y0 ) * t;\n'
    b'\t\tif ( !doc->bodyAtWorld( wx, wy ) )\n'
    b'\t\t\tcontinue;\n'
    b'\t\tWaterStrokePoint p;\n'
)
P_LAY_NEW = (
    b'\tconst int steps = 64;\n'
    b'\t/* AS DRAWN.  This used to drop every point that was not on water, so a\n'
    b'\t * stroke laid on dry land reached the model EMPTY and came back "that\n'
    b'\t * stroke has no points" instead of the refusal the spec asks for -- and\n'
    b'\t * the file no longer held what the user actually drew.  The model names\n'
    b'\t * the body from the first point that lands on water and reports how many\n'
    b'\t * missed, which is the same forgiveness said out loud. */\n'
    b'\tfor ( int k = 0; k <= steps; k++ ) {\n'
    b'\t\tconst double t = double( k ) / steps;\n'
    b'\t\tconst double wx = x0 + ( x1 - x0 ) * t;\n'
    b'\t\tconst double wy = y0 + ( y1 - y0 ) * t;\n'
    b'\t\tWaterStrokePoint p;\n'
)

EDITS = [
    ('src/watermark.cpp', [(C_SAVE_ANCHOR, C_SAVE_NEW), (C_SAVE2_ANCHOR, C_SAVE2_NEW),
                           (C_ADD_ANCHOR, C_ADD_NEW)]),
    ('src/watermarkpanel.cpp', [(P_LAY_ANCHOR, P_LAY_NEW)]),
]

bad = 0
for rel, edits in EDITS:
    path = os.path.join(ROOT, rel)
    with open(path, 'rb') as f:
        data = f.read()
    print('%-26s %s %8d bytes %6d lines CR=%d' % (
        rel, hashlib.sha1(data).hexdigest()[:16], len(data),
        data.count(b'\n'), data.count(b'\r')))
    cr0 = data.count(b'\r')
    hurt = False
    for anchor, new in edits:
        n = data.count(anchor)
        if n != 1:
            print('  REFUSED: %d matches for %r' % (n, anchor[:70]))
            bad += 1
            hurt = True
            continue
        before = len(data)
        data = data.replace(anchor, new, 1)
        print('  ok: %d -> %d bytes  (+%d)' % (before, len(data), len(data) - before))
    if data.count(b'\r') != cr0:
        print('  REFUSED: CR moved %d -> %d' % (cr0, data.count(b'\r')))
        bad += 1
        hurt = True
    if not check_only and not hurt:
        with open(path, 'wb') as f:
            f.write(data)
        print('  written: %d bytes %d lines CR=%d' % (
            len(data), data.count(b'\n'), data.count(b'\r')))

if bad:
    print('REFUSED (%d)' % bad)
    sys.exit(1)
if check_only:
    print('--check: nothing written')
sys.exit(0)
