#!/usr/bin/env python
# Lane SKELOVERLAY, third pass on src/glview.cpp (CRLF): the overlay LOOKS UP
# nodes instead of creating them.  Scene::getNode() constructs a Node for any
# block handed to it; the overlay is handed every NiAVObject block in the file,
# so on a file with a block the graph does not reach it would have grown the
# scene -- and Scene::bounds() with it -- while claiming to be read-only.
# Only the two NEW functions change; poseBoneTail()'s path is left exactly as
# it was, because every bone IT is given is a graph node already and a
# behaviour change there is not this lane's to make.
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src', 'glview.cpp')

def crlf(s):
    return s.replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8')

EDITS = [
('refresh-lookup',
'''		if ( !scene->getNode( model, model->getBlockIndex( b.block ) ) ) {
''',
'''		if ( !scene->findNode( model, model->getBlockIndex( b.block ) ) ) {
'''),
('draw-lookup',
'''	for ( int b : skelOverlayBones )
		if ( Node * n = scene->getNode( model, model->getBlockIndex( b ) ) )
			head.insert( b, n->worldTrans().translation );
''',
'''	for ( int b : skelOverlayBones )
		if ( Node * n = scene->findNode( model, model->getBlockIndex( b ) ) )
			head.insert( b, n->worldTrans().translation );
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
    print('dCR %+d  dLF %+d' % (out.count(b'\r') - cr0, out.count(b'\n') - lf0))
    if check:
        print('--check: nothing written')
        return 0
    open(SRC, 'wb').write(out)
    print('written')
    return 0

sys.exit(main())
