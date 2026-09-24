"""INCR1 step 3d -- the objectCorpusHash refusal names the plugin too.

`lodgen_bakerec.sh` leg (e) is red on TWO checks, and the reason is not the
volatile field the brief attributed it to: of the three staleness branches in
`lodgenNativeVerify()` (src/nativeemit.cpp) the pluginCorpusHash and
loadOrderHash ones append `lodbNameTheStalePlugin()` and the objectCorpusHash
one -- the branch an edited REFR actually takes, and therefore the branch the
gate exercises -- does not. One call, three branches, two of them wired.
"""
import io
import os
import sys

NL = chr(10)
TAB = chr(9)
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
CHECK = '--check' in sys.argv[1:]
rel = 'src/nativeemit.cpp'

with io.open(os.path.join(ROOT, rel), 'rb') as fh:
    b = fh.read()
assert b.count(chr(13).encode()) == 0, rel + ' is not LF-only'
t = b.decode('utf-8')

old = (TAB + TAB + TAB + TAB + '"base' + chr(39) + 's MNAM or a SCOL part changed since the bake" )' + NL
       + TAB + TAB + TAB + TAB + '.arg( lodoPath ).arg( lh.objectCorpusHash, 16, 16, QChar( ' + chr(39) + '0' + chr(39)
       + ' ) ).arg( objHash, 16, 16, QChar( ' + chr(39) + '0' + chr(39) + ' ) )' + NL
       + TAB + TAB + TAB + TAB + '.arg( world->pluginList() ) );')
new = (TAB + TAB + TAB + TAB + '"base' + chr(39) + 's MNAM or a SCOL part changed since the bake" )' + NL
       + TAB + TAB + TAB + TAB + '.arg( lodoPath ).arg( lh.objectCorpusHash, 16, 16, QChar( ' + chr(39) + '0' + chr(39)
       + ' ) ).arg( objHash, 16, 16, QChar( ' + chr(39) + '0' + chr(39) + ' ) )' + NL
       + TAB + TAB + TAB + TAB + '.arg( world->pluginList() )' + NL
       + TAB + TAB + TAB + TAB + '/* ...and NAME the plugin, as the other two staleness branches'
       + ' below do.' + NL
       + TAB + TAB + TAB + TAB + ' * This is the branch an edited REFR takes -- the common case, and'
       + NL
       + TAB + TAB + TAB + TAB + ' * the one `lodgen_bakerec.sh` leg (e) exercises -- and it was the'
       + NL
       + TAB + TAB + TAB + TAB + ' * one of the three that did not call it (lane INCR1, 2026-09-17).'
       + ' */' + NL
       + TAB + TAB + TAB + TAB + '+ lodbNameTheStalePlugin( lodoPath, world ) );')

n = t.count(old)
if n != 1:
    raise SystemExit('ANCHOR objectCorpusHash refusal: found %d times, wanted 1' % n)
if 'the one of the three that did not call it' in t:
    raise SystemExit(rel + ' already patched')
t = t.replace(old, new, 1)

if CHECK:
    print('  would write %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
else:
    with io.open(os.path.join(ROOT, rel), 'wb') as fh:
        fh.write(t.encode('utf-8'))
    print('  wrote %s (%d bytes)' % (rel, len(t.encode('utf-8'))))
