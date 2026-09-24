#!/usr/bin/env python
"""IMPOSTORFIX1 fix02: repair the octahedral card's HEIGHT channel at bake time.

Adds `lodgenRepairOctHeight` after `lodgenDilateFrames` and calls it at both
sites that dilate an octahedral `_n` sheet (the per-card DDS writer and the
atlas builder), immediately AFTER the dilate -- the dilate is what floods the
height outside the silhouette with the frame's average, so a repair before it
would be overwritten.
"""
import io

P = 'src/lodgen.cpp'
b = io.open(P, 'rb').read()
cr = b.count(b'\r')
assert cr == 0, cr

snippet = io.open('scratchpad/impostorfix1_20260919/snippet_repair.txt', 'rb').read()
snippet = snippet.replace(b'\r\n', b'\n')

# --- 1. the function, right after lodgenDilateFrames ends -------------------
ANCHOR = b"""//! One BC4 block (the BC3 alpha block by another name): eight-step palette over the block's min and max.
static void lodgenEncodeBC4Block("""
assert b.count(ANCHOR) == 1, b.count(ANCHOR)
b = b.replace(ANCHOR, snippet.lstrip(b'\n') + b'\n' + ANCHOR)

# --- 2. call site A: the per-card DDS writer -------------------------------
OLD_A = b"""				lodgenDilateFrames( alb, alb, card.octTileW, card.octTileH, deep );	// last: it is also the coverage
"""
NEW_A = b"""				lodgenDilateFrames( alb, alb, card.octTileW, card.octTileH, deep );	// last: it is also the coverage
				/* AFTER the dilate, never before: the dilate is what floods the
				 * height outside the silhouette with the frame's average. */
				{
					QString heightReport;
					lodgenRepairOctHeight( nrm, alb, card.octTileW, card.octTileH, &heightReport );
					if ( !heightReport.isEmpty() )
						fprintf( stderr, "lodgen: card %s: %s\\n", id.toLocal8Bit().constData(),
							heightReport.toLocal8Bit().constData() );
				}
"""
assert b.count(OLD_A) == 1, b.count(OLD_A)
b = b.replace(OLD_A, NEW_A)

# --- 3. call site B: the atlas builder -------------------------------------
OLD_B = b"""		lodgenDilateFrames( nrm, alb, fw, fh, deep );
		lodgenDilateFrames( rm, alb, fw, fh, deep );
		lodgenDilateFrames( emi, alb, fw, fh, deep );
		lodgenDilateFrames( alb, alb, fw, fh, deep );
"""
NEW_B = b"""		lodgenDilateFrames( nrm, alb, fw, fh, deep );
		lodgenDilateFrames( rm, alb, fw, fh, deep );
		lodgenDilateFrames( emi, alb, fw, fh, deep );
		lodgenDilateFrames( alb, alb, fw, fh, deep );
		{
			QString heightReport;
			lodgenRepairOctHeight( nrm, alb, fw, fh, &heightReport );
			if ( !heightReport.isEmpty() )
				fprintf( stderr, "lodgen: arrays: card %s: %s\\n", id.toLocal8Bit().constData(),
					heightReport.toLocal8Bit().constData() );
		}
"""
assert b.count(OLD_B) == 1, b.count(OLD_B)
b = b.replace(OLD_B, NEW_B)

assert b.count(b'\r') == cr
io.open(P, 'wb').write(b)
print('fix02 applied, %d bytes, CR=%d' % (len(b), b.count(b'\r')))
