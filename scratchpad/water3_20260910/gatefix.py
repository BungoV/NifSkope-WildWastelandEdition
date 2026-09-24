# -*- coding: utf-8 -*-
"""gatefix.py -- lane BUILD5b: the five red gates of the 02:32 run.

The first run of `tests/spells/water_mark.sh` on the built exe printed
5 failures.  Diagnosed, they are FOUR causes, three of them in the harness
itself and one in the tool:

  A  the "dry land" control was placed at the worldspace's own corner, on the
     assumption that a corner is dry.  On the Commonwealth the corner is OPEN
     SEA (body 1, 21,575,619 texels), so the stroke was correctly ACCEPTED and
     the control read as a failure.  Fixed by finding a dry point IN THE FILE
     (CONSTITUTION rule 4: the reference is measured, never assumed).  Both
     halves.

  B  because A's stroke was accepted, body 1 -- the sea -- carried a stroke for
     the rest of the run.  That is what grew the file by 66,533 bytes between
     the first and the second save (gate P8): save() re-solves, the sea's flow
     plane stopped being uniform, and a uniform tile costs 4 bytes while a
     solved one costs thousands.  A cascade of A, not a separate defect.

  C  THE ONE REAL DEFECT.  solve() writes flowX/flowY/flowSource/confidence/
     source/outlet and flag bits 0 and 1 into the body table and NOTHING ever
     puts them back, so removing a stroke could not reproduce the file it
     started from -- gate P3 failed by 1,021,405 bytes, which is the sea's
     share of the flow plane re-derived from a mean a solve had moved.  The
     strokes are the source, so these fields are DERIVED and must be
     re-derived from scratch on every solve.  Fixed by keeping the table as it
     was read and restoring the derived fields at the top of solve(); and
     solve() stops setting flag bit 0 ("user-edited"), because it cannot
     un-set a bit the panel's own setters also set without wiping their edit.

  D  the harness's stroke ran down the body's BOUNDING-BOX DIAGONAL and kept
     every point that was on ANY water, so 13 of its 16 points were not on the
     river at all, and the diagonal of a bbox is close to the body's own mean
     direction (55.5 vs 54.84 degrees) -- which is why only 38.5 per cent of
     the river's texels moved against a floor of 60.  Replaced by a real
     centreline: the mean position of the body's own texels in each slice
     across its longer axis.  Whether that clears the pre-registered 60 is a
     MEASUREMENT, not a promise -- see the lane report.

Every edit refuses unless its anchor matches EXACTLY ONCE, and every file
touched is LF-only (CR must stay 0).

    python scratchpad/water3_20260910/gatefix.py --check
    python scratchpad/water3_20260910/gatefix.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv

# ---------------------------------------------------------------- watermark.h
H_GEOM_ANCHOR = b'\tvoid texelToWorld( int px, int py, double & wx, double & wy ) const;\n'
H_GEOM_NEW = H_GEOM_ANCHOR + (
    b'\t/*! A two-point segment entirely on DRY LAND, found in THIS file rather\n'
    b'\t *  than assumed -- the control for addStroke()\'s refusal.  A worldspace\n'
    b'\t *  corner is not dry on the Commonwealth, it is open sea, so the point\n'
    b'\t *  has to be measured.  False when the file carries no dry texel with a\n'
    b'\t *  dry neighbour, which is itself worth saying out loud. */\n'
    b'\tbool dryStroke( double & x0, double & y0, double & x1, double & y1 ) const;\n'
)

H_MEMBER_ANCHOR = (
    b'\tQByteArray originalTable;        //!< the table\'s bytes as read, for the repack gate\n'
)
H_MEMBER_NEW = H_MEMBER_ANCHOR + (
    b'\t/*! The body table as it was READ.  flowX, flowY, flowSource, confidence,\n'
    b'\t *  source, outlet and flag bit 1 are DERIVED from the strokes, so every\n'
    b'\t *  solve puts these back before deriving them again: without it they\n'
    b'\t *  accumulated and removing a stroke could not reproduce the file it\n'
    b'\t *  started from (gate P3, which failed by 1,021,405 bytes). */\n'
    b'\tQVector<LodtWaterBody> tableAtOpen;\n'
)

# -------------------------------------------------------------- watermark.cpp
C_OPEN_ANCHOR = (
    b'\t\toriginalTable = f.read( qint64( table.size() ) * bodyStride );\n'
    b'\t}\n'
)
C_OPEN_NEW = C_OPEN_ANCHOR + (
    b'\t/* And the same table as RECORDS, for solve() to restore the fields it\n'
    b'\t *  derives.  Taken here, after the locks and the strokes are read and\n'
    b'\t *  before anything has solved, so it is the writer\'s own answer. */\n'
    b'\ttableAtOpen = table;\n'
)

C_SOLVE_ANCHOR = (
    b'\tqDeleteAll( fields );\n'
    b'\tfields.clear();\n'
    b'\n'
    b'\t// which bodies carry a constraint at all\n'
)
C_SOLVE_NEW = (
    b'\tqDeleteAll( fields );\n'
    b'\tfields.clear();\n'
    b'\n'
    b'\t/* PUT BACK WHAT AN EARLIER SOLVE DERIVED.  The strokes are the source, so\n'
    b'\t * flowX, flowY, flowSource, confidence, source, outlet and flag bit 1 are\n'
    b'\t * re-derived from scratch every time and never accumulated.  Without this\n'
    b'\t * a body that was marked once kept its stroke\'s mean for the rest of the\n'
    b'\t * document\'s life, and since the flow plane is derived from that mean,\n'
    b'\t * removing the stroke could not reproduce the file it started from -- gate\n'
    b'\t * P3 failed by 1,021,405 bytes, the sea\'s share of the plane.  The fields\n'
    b'\t * the PANEL owns -- name, class, colour, water form and flag bits 0, 2\n'
    b'\t * and 5 -- are deliberately not touched here. */\n'
    b'\tfor ( int i = 0; i < table.size() && i < tableAtOpen.size(); i++ ) {\n'
    b'\t\tconst LodtWaterBody & o = tableAtOpen[i];\n'
    b'\t\tLodtWaterBody & r = table[i];\n'
    b'\t\tr.flowX = o.flowX;\n'
    b'\t\tr.flowY = o.flowY;\n'
    b'\t\tr.flowSource = o.flowSource;\n'
    b'\t\tr.confidence = o.confidence;\n'
    b'\t\tr.source = o.source;\n'
    b'\t\tr.outlet = o.outlet;\n'
    b'\t\tr.flags = quint8( ( r.flags & ~quint8( 1u << 1 ) ) | ( o.flags & quint8( 1u << 1 ) ) );\n'
    b'\t}\n'
    b'\n'
    b'\t// which bodies carry a constraint at all\n'
)

C_FLAG_ANCHOR = b'\t\trec.flags |= quint8( ( 1u << 0 ) | ( 1u << 1 ) );\n'
C_FLAG_NEW = (
    b'\t\t/* Bit 1 ("flow from a stroke") only.  Bit 0 ("user-edited") belongs to\n'
    b'\t\t * the explicit setters -- name, class, colour -- because solve() cannot\n'
    b'\t\t * un-set it when the stroke is removed without wiping THEIR edit, and a\n'
    b'\t\t * bit that can be set but never cleared breaks the undo gate. */\n'
    b'\t\trec.flags |= quint8( 1u << 1 );\n'
)

C_DRY_ANCHOR = (
    b'quint16 WaterMarkDoc::bodyAtWorld( double wx, double wy ) const\n'
)
C_DRY_NEW = (
    b'/*! A segment on dry land, MEASURED.\n'
    b' *\n'
    b' *  The obvious candidate -- a corner of the worldspace -- is open sea on\n'
    b' *  the Commonwealth, so a control written on it tests nothing and reads as\n'
    b' *  a failure of the refusal it was meant to prove.  This walks the world on\n'
    b' *  a half-cell grid and takes the first place where the start, the middle\n'
    b' *  and the end of a quarter-cell segment are all dry. */\n'
    b'bool WaterMarkDoc::dryStroke( double & x0, double & y0, double & x1, double & y1 ) const\n'
    b'{\n'
    b'\tx0 = y0 = x1 = y1 = 0.0;\n'
    b'\tif ( !opened )\n'
    b'\t\treturn false;\n'
    b'\tdouble wx0 = 0.0, wy0 = 0.0, wx1 = 0.0, wy1 = 0.0;\n'
    b'\tworldBounds( wx0, wy0, wx1, wy1 );\n'
    b'\tconst double step = kCellUnits * 0.5;\n'
    b'\tconst double len = kCellUnits * 0.25;\n'
    b'\tfor ( double y = wy0 + step; y < wy1; y += step ) {\n'
    b'\t\tfor ( double x = wx0 + step; x + len < wx1; x += step ) {\n'
    b'\t\t\tif ( bodyAtWorld( x, y ) || bodyAtWorld( x + len * 0.5, y )\n'
    b'\t\t\t\t|| bodyAtWorld( x + len, y ) )\n'
    b'\t\t\t\tcontinue;\n'
    b'\t\t\tx0 = x;\n'
    b'\t\t\ty0 = y;\n'
    b'\t\t\tx1 = x + len;\n'
    b'\t\t\ty1 = y;\n'
    b'\t\t\treturn true;\n'
    b'\t\t}\n'
    b'\t}\n'
    b'\treturn false;\n'
    b'}\n'
    b'\n'
) + C_DRY_ANCHOR

# ---- the harness: a real centreline, and a dry point found in the file ------
C_AXIS_ANCHOR = (
    b'\tauto axisStroke = [&]( const LodtWaterBody & b ) {\n'
    b'\t\t/* From one end of the body\'s bounding box to the other along its longer\n'
    b'\t\t * side, sampled every cell, and every point that misses the body is\n'
    b'\t\t * dropped -- so the stroke is a real line down the water, not a\n'
    b'\t\t * diagonal through the land beside it. */\n'
    b'\t\tWaterStroke s;\n'
    b'\t\ts.kind = WaterStroke::Stroke;\n'
    b'\t\ts.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;\n'
    b'\t\ts.speed = 0.5f;\n'
    b'\t\ts.width = 4096.0f;\n'
    b'\t\tconst double x0 = ( double( b.x0 ) + 0.5 ) * kCellUnits;\n'
    b'\t\tconst double x1 = ( double( b.x1 ) + 0.5 ) * kCellUnits;\n'
    b'\t\tconst double y0 = ( double( b.y0 ) + 0.5 ) * kCellUnits;\n'
    b'\t\tconst double y1 = ( double( b.y1 ) + 0.5 ) * kCellUnits;\n'
    b'\t\tconst int steps = 64;\n'
    b'\t\tfor ( int k = 0; k <= steps; k++ ) {\n'
    b'\t\t\tconst double t = double( k ) / steps;\n'
    b'\t\t\tconst double wx = x0 + ( x1 - x0 ) * t;\n'
    b'\t\t\tconst double wy = y0 + ( y1 - y0 ) * t;\n'
    b'\t\t\tif ( !doc.bodyAtWorld( wx, wy ) )\n'
    b'\t\t\t\tcontinue;\n'
    b'\t\t\tWaterStrokePoint p;\n'
    b'\t\t\tp.x = float( wx );\n'
    b'\t\t\tp.y = float( wy );\n'
    b'\t\t\ts.pts.append( p );\n'
    b'\t\t}\n'
    b'\t\treturn s;\n'
    b'\t};\n'
)
C_AXIS_NEW = (
    b'\tauto axisStroke = [&]( const LodtWaterBody & b ) {\n'
    b'\t\t/* THE BODY\'S OWN CENTRELINE, not its bounding-box diagonal.\n'
    b'\t\t *\n'
    b'\t\t * The first version of this walked the bbox diagonal and kept every\n'
    b'\t\t * point that was on ANY water: 13 of its 16 points were not on the\n'
    b'\t\t * river at all, and a bbox diagonal is within a degree of the body\'s\n'
    b'\t\t * own mean direction (55.5 against 54.84 on the Charles), so the\n'
    b'\t\t * stroke asked the plane for what it already said.  This slices the\n'
    b'\t\t * body across its LONGER side and takes the mean position of its own\n'
    b'\t\t * texels in each slice, which follows a winding reach. */\n'
    b'\t\tWaterStroke s;\n'
    b'\t\ts.kind = WaterStroke::Stroke;\n'
    b'\t\ts.flags = WaterStroke::SetsDirection | WaterStroke::SetsSpeed;\n'
    b'\t\ts.speed = 0.5f;\n'
    b'\t\ts.width = 4096.0f;\n'
    b'\t\tint px0 = 0, py0 = 0, px1 = 0, py1 = 0;\n'
    b'\t\tdoc.worldToTexel( double( b.x0 ) * kCellUnits, double( b.y0 ) * kCellUnits, px0, py0 );\n'
    b'\t\tdoc.worldToTexel( ( double( b.x1 ) + 1.0 ) * kCellUnits,\n'
    b'\t\t\t( double( b.y1 ) + 1.0 ) * kCellUnits, px1, py1 );\n'
    b'\t\tconst bool alongY = ( py1 - py0 ) >= ( px1 - px0 );\n'
    b'\t\tconst int a0 = alongY ? py0 : px0, a1 = alongY ? py1 : px1;\n'
    b'\t\tconst int b0 = alongY ? px0 : py0, b1 = alongY ? px1 : py1;\n'
    b'\t\tfor ( int a = a0; a <= a1; a++ ) {\n'
    b'\t\t\tdouble sx = 0.0, sy = 0.0;\n'
    b'\t\t\tqint64 n = 0;\n'
    b'\t\t\tfor ( int c = b0; c <= b1; c++ ) {\n'
    b'\t\t\t\tconst int px = alongY ? c : a, py = alongY ? a : c;\n'
    b'\t\t\t\tdouble wx = 0.0, wy = 0.0;\n'
    b'\t\t\t\tdoc.texelToWorld( px, py, wx, wy );\n'
    b'\t\t\t\tif ( doc.bodyAtWorld( wx, wy ) != b.id )\n'
    b'\t\t\t\t\tcontinue;\n'
    b'\t\t\t\tsx += wx;\n'
    b'\t\t\t\tsy += wy;\n'
    b'\t\t\t\tn++;\n'
    b'\t\t\t}\n'
    b'\t\t\tif ( !n )\n'
    b'\t\t\t\tcontinue;\n'
    b'\t\t\tWaterStrokePoint p;\n'
    b'\t\t\tp.x = float( sx / double( n ) );\n'
    b'\t\t\tp.y = float( sy / double( n ) );\n'
    b'\t\t\tif ( !s.pts.isEmpty() ) {\n'
    b'\t\t\t\t// one point a cell is plenty; the solve interpolates the segment\n'
    b'\t\t\t\tconst double dx = double( p.x ) - double( s.pts.last().x );\n'
    b'\t\t\t\tconst double dy = double( p.y ) - double( s.pts.last().y );\n'
    b'\t\t\t\tif ( dx * dx + dy * dy < kCellUnits * kCellUnits * 0.25 )\n'
    b'\t\t\t\t\tcontinue;\n'
    b'\t\t\t}\n'
    b'\t\t\ts.pts.append( p );\n'
    b'\t\t}\n'
    b'\t\treturn s;\n'
    b'\t};\n'
)

C_DRYCASE_ANCHOR = (
    b'\t{\n'
    b'\t\tdouble x0 = 0, y0 = 0, x1 = 0, y1 = 0;\n'
    b'\t\tdoc.worldBounds( x0, y0, x1, y1 );\n'
    b'\t\tWaterStroke s;\n'
    b'\t\ts.kind = WaterStroke::Stroke;\n'
    b'\t\t// the worldspace\'s own corner: as dry as this file gets\n'
    b'\t\tWaterStrokePoint a, b;\n'
    b'\t\ta.x = float( x0 + 8.0 );\n'
    b'\t\ta.y = float( y0 + 8.0 );\n'
    b'\t\tb.x = float( x0 + 4096.0 );\n'
    b'\t\tb.y = float( y0 + 8.0 );\n'
    b'\t\ts.pts << a << b;\n'
)
C_DRYCASE_NEW = (
    b'\t{\n'
    b'\t\t/* The dry point is FOUND IN THE FILE.  The worldspace corner this case\n'
    b'\t\t * used to assume was dry is open sea on the Commonwealth (body 1), so\n'
    b'\t\t * the stroke was rightly accepted and the control read as a failure of\n'
    b'\t\t * the refusal it exists to prove. */\n'
    b'\t\tdouble x0 = 0, y0 = 0, x1 = 0, y1 = 0;\n'
    b'\t\tconst bool haveDry = doc.dryStroke( x0, y0, x1, y1 );\n'
    b'\t\tsay( QStringLiteral( "the dry point is at (%1, %2) -> (%3, %4), and body %5 sits "\n'
    b'\t\t\t"at the worldspace corner" ).arg( x0, 0, \'f\', 0 ).arg( y0, 0, \'f\', 0 )\n'
    b'\t\t\t.arg( x1, 0, \'f\', 0 ).arg( y1, 0, \'f\', 0 )\n'
    b'\t\t\t.arg( doc.bodyAtWorld( wbx0 + 8.0, wby0 + 8.0 ) ) );\n'
    b'\t\tcheck( QStringLiteral( "a dry point was found in the file, not assumed" ), haveDry );\n'
    b'\t\tWaterStroke s;\n'
    b'\t\ts.kind = WaterStroke::Stroke;\n'
    b'\t\tWaterStrokePoint a, b;\n'
    b'\t\ta.x = float( x0 );\n'
    b'\t\ta.y = float( y0 );\n'
    b'\t\tb.x = float( x1 );\n'
    b'\t\tb.y = float( y1 );\n'
    b'\t\ts.pts << a << b;\n'
)

# the corner body is quoted in the sentence above, so the bounds are needed there
C_BOUNDS_ANCHOR = b'\t// ---- 6. a stroke on dry land is refused in words ------------------------\n'
C_BOUNDS_NEW = (
    b'\tdouble wbx0 = 0.0, wby0 = 0.0, wbx1 = 0.0, wby1 = 0.0;\n'
    b'\tdoc.worldBounds( wbx0, wby0, wbx1, wby1 );\n'
) + C_BOUNDS_ANCHOR

# --------------------------------------------------------- watermarkpanel.cpp
P_DRY_ANCHOR = (
    b'\t\tdouble bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;\n'
    b'\t\tdoc->worldBounds( bx0, by0, bx1, by1 );\n'
    b'\t\tconst QString dry = panel->view()->layStroke( bx0 + 8.0, by0 + 8.0, bx0 + 4096.0, by0 + 8.0 );\n'
)
P_DRY_NEW = (
    b'\t\tdouble bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;\n'
    b'\t\t/* Found in the file, never assumed: the worldspace corner is open sea\n'
    b'\t\t * on the Commonwealth, so a stroke laid there is correctly ACCEPTED. */\n'
    b'\t\tconst bool haveDry = doc->dryStroke( bx0, by0, bx1, by1 );\n'
    b'\t\tlog << "the dry point found in the file is (" << bx0 << ", " << by0 << ")\\n";\n'
    b'\t\tcheck( QStringLiteral( "a dry point was found in the file, not assumed" ), haveDry );\n'
    b'\t\tconst QString dry = panel->view()->layStroke( bx0, by0, bx1, by1 );\n'
)

EDITS = [
    ('src/watermark.h', [(H_GEOM_ANCHOR, H_GEOM_NEW), (H_MEMBER_ANCHOR, H_MEMBER_NEW)]),
    ('src/watermark.cpp', [(C_OPEN_ANCHOR, C_OPEN_NEW), (C_SOLVE_ANCHOR, C_SOLVE_NEW),
                           (C_FLAG_ANCHOR, C_FLAG_NEW), (C_DRY_ANCHOR, C_DRY_NEW),
                           (C_AXIS_ANCHOR, C_AXIS_NEW), (C_BOUNDS_ANCHOR, C_BOUNDS_NEW),
                           (C_DRYCASE_ANCHOR, C_DRYCASE_NEW)]),
    ('src/watermarkpanel.cpp', [(P_DRY_ANCHOR, P_DRY_NEW)]),
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
    for anchor, new in edits:
        n = data.count(anchor)
        if n != 1:
            print('  REFUSED: %d matches for %r' % (n, anchor[:70]))
            bad += 1
            continue
        before = len(data)
        data = data.replace(anchor, new, 1)
        print('  ok: %d -> %d bytes  (+%d)' % (before, len(data), len(data) - before))
    if data.count(b'\r') != cr0:
        print('  REFUSED: CR moved %d -> %d' % (cr0, data.count(b'\r')))
        bad += 1
        continue
    if not check_only and not bad:
        with open(path, 'wb') as f:
            f.write(data)
        print('  written: %d bytes %d lines CR=%d' % (
            len(data), data.count(b'\n'), data.count(b'\r')))

if bad:
    print('REFUSED (%d) -- nothing was written past the refusal' % bad)
    sys.exit(1)
if check_only:
    print('--check: nothing written')
sys.exit(0)
