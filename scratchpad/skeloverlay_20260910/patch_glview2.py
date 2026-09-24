#!/usr/bin/env python
# Lane SKELOVERLAY, second pass on src/glview.cpp (CRLF): record the segments
# the overlay actually draws, so the pixel-difference gate's mask is a readback
# and not a re-derivation of where the lines "should" be.
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src', 'glview.cpp')

def crlf(s):
    return s.replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8')

EDITS = [
('clear-segs',
'''	skelOverlayDrawnAt.clear();
	skelOverlayDrawnAt.reserve( skelOverlayBones.size() );
''',
'''	skelOverlayDrawnAt.clear();
	skelOverlayDrawnAt.reserve( skelOverlayBones.size() );
	skelOverlayDrawnSegs.clear();
'''),
('body-seg',
'''		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
		drawOctahedralBone( head.value( p ), head.value( b ) );
		c.segments++;
''',
'''		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
		drawOctahedralBone( head.value( p ), head.value( b ) );
		skelOverlayDrawnSegs.append( qMakePair( head.value( p ), head.value( b ) ) );
		c.segments++;
'''),
('stub-seg',
'''		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
		drawOctahedralBone( head.value( b ), boneTailIn( b, skelOverlayBones, cap ) );
		c.stubs++;
''',
'''		const Vector3 tail = boneTailIn( b, skelOverlayBones, cap );
		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
		drawOctahedralBone( head.value( b ), tail );
		skelOverlayDrawnSegs.append( qMakePair( head.value( b ), tail ) );
		c.stubs++;
'''),
('clear-off',
'''		skelOverlayBones.clear();
		skelOverlayClass.clear();
		skelOverlayDrawnAt.clear();
		skelOverlayCensus = SkeletonOverlayCensus();
''',
'''		skelOverlayBones.clear();
		skelOverlayClass.clear();
		skelOverlayDrawnAt.clear();
		skelOverlayDrawnSegs.clear();
		skelOverlayCensus = SkeletonOverlayCensus();
'''),
]

def main():
    check = '--check' in sys.argv
    data = open(SRC, 'rb').read()
    cr0, lf0 = data.count(b'\r'), data.count(b'\n')
    out = data
    bad = []
    for name, old, new in EDITS:
        o, n = crlf(old), crlf(new)
        if out.count(o) != 1:
            bad.append('%s: anchor found %d times' % (name, out.count(o)))
            continue
        out = out.replace(o, n, 1)
    if bad:
        print('REFUSED, nothing written:')
        for x in bad:
            print('  ' + x)
        return 2
    cr1, lf1 = out.count(b'\r'), out.count(b'\n')
    d = lf1 - lf0
    print('lines added: %d   dCR %+d' % (d, cr1 - cr0))
    if cr1 - cr0 != d:
        print('REFUSED: bare LF got in')
        return 3
    if check:
        print('--check: nothing written')
        return 0
    open(SRC, 'wb').write(out)
    print('written')
    return 0

sys.exit(main())
