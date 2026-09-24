"""Lane CELLVIEW4B, fix 02 -- the gate hook that lets the 12M refusal be PROVEN
instead of argued, and nothing else in src/.

`cellSplatCountVerts()` runs before any allocation and the blend refuses past
`CELL_MAX_TOTAL_VERTS` (12,000,000).  Proving that at runtime means asking for a
rectangle big enough to blow the cap -- Sanctuary -20,7 costs 8,936 land
vertices, so the smallest honest block is about 38x38 cells, which is minutes of
LAND and REFR reading for one boolean.  A gate that expensive does not get run,
and a refusal path nobody runs is a refusal path nobody knows works.

`WW_CELL_SPLAT_CAP` lowers the cap for one run.  It is a HARNESS HOOK, in the
same family as WW_CELL_NOTERRAIN / NOWATER / NOGRID / MARKERS / DISABLED which
already exist in this file and exist for the same reason: it forces a state the
gate needs to measure.  It is not a feature switch and it is not a way back from
anything -- unset, the cap is 12,000,000 exactly as before, and the gate pins
that too.
"""
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CHECK = '--check' in sys.argv


def edit(rel, anchor, new):
    path = os.path.join(ROOT, rel)
    with io.open(path, 'rb') as fh:
        raw = fh.read()
    cr_before = raw.count(b'\r')
    txt = raw.decode('utf-8')
    n = txt.count(anchor)
    print('%-20s %d  %r' % (rel, n, anchor[:58]))
    assert n == 1, 'anchor matched %d times, not once' % n
    out = txt.replace(anchor, new)
    if CHECK:
        return
    data = out.encode('utf-8')
    assert data.count(b'\r') == cr_before, 'CR count moved'
    with io.open(path, 'wb') as fh:
        fh.write(data)


edit('src/cellview.cpp',
'''		QString splatNote;   // carried past the mosaic fallback (lane CELLVIEW4B)
		if ( spec.terrain ) {
			const qint64 splatVerts = cellSplatCountVerts( world, x0, y0, x1, y1 );''',
'''		QString splatNote;   // carried past the mosaic fallback (lane CELLVIEW4B)
		/* THE CAP, AND THE HARNESS HOOK THAT LETS IT BE PROVEN (lane CELLVIEW4B).
		 * Unset, this is CELL_MAX_TOTAL_VERTS and nothing has changed. Set, the
		 * blend refuses at a rectangle a gate can actually afford to open:
		 * Sanctuary -20,7 costs 8,936 land vertices, so proving the 12,000,000
		 * cap honestly would mean a 38x38-cell block, and a refusal path that is
		 * too expensive to run is a refusal path nobody knows works. Same family
		 * as WW_CELL_NOTERRAIN / NOWATER / NOGRID above: it forces the state the
		 * measurement needs. It is not a feature switch and not a way back. */
		qint64 splatCap = qint64( CELL_MAX_TOTAL_VERTS );
		{
			bool capOk = false;
			const qint64 v = qEnvironmentVariableIntValue( "WW_CELL_SPLAT_CAP", &capOk );
			if ( capOk && v > 0 )
				splatCap = v;
		}
		if ( spec.terrain ) {
			const qint64 splatVerts = cellSplatCountVerts( world, x0, y0, x1, y1 );''')

edit('src/cellview.cpp',
'''			splatNote = QStringLiteral( "; %L1 land vertices counted before "
				"allocating, against the %L2 cap" )
				.arg( splatVerts ).arg( qint64( CELL_MAX_TOTAL_VERTS ) );''',
'''			splatNote = QStringLiteral( "; %L1 land vertices counted before "
				"allocating, against the %L2 cap" )
				.arg( splatVerts ).arg( splatCap );''')

edit('src/cellview.cpp',
'''			if ( splatVerts > 0 && splatVerts <= CELL_MAX_TOTAL_VERTS ) {''',
'''			if ( splatVerts > 0 && splatVerts <= splatCap ) {''')

edit('src/cellview.cpp',
'''			} else if ( splatVerts > CELL_MAX_TOTAL_VERTS ) {''',
'''			} else if ( splatVerts > splatCap ) {''')

edit('src/cellview.cpp',
'''				splatNote = QStringLiteral( "ground: the BLEND REFUSED -- %L1 vertices "
					"is past the %L2 cap; the hard-edged mosaic was drawn instead. " )
					.arg( splatVerts ).arg( qint64( CELL_MAX_TOTAL_VERTS ) );''',
'''				splatNote = QStringLiteral( "ground: the BLEND REFUSED -- %L1 vertices "
					"is past the %L2 cap; the hard-edged mosaic was drawn instead. " )
					.arg( splatVerts ).arg( splatCap );''')

print()
print('CHECK ONLY, nothing written' if CHECK else 'APPLIED')
