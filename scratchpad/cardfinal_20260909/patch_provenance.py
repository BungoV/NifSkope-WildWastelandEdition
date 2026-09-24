#!/usr/bin/env python3
"""Provenance rows for the two contract pages: repair the two anchors this lane's
own edits invalidated, correct the claims the law changed, and add rows for the
new fields. The line NUMBERS are then re-derived by `anchors.py --apply`.
"""
import hashlib
import sys

CS = 'docs/LODGEN_CARD_SHEETS.md'
LF = 'docs/LODGEN_LODM_FORMAT.md'


def edit(path, pairs):
    s = open(path, encoding='utf-8').read()
    for old, new in pairs:
        n = s.count(old)
        if n != 1:
            print('%s: anchor matched %d times: %r' % (path, n, old[:70]))
            sys.exit(1)
        s = s.replace(old, new)
    open(path, 'w', encoding='utf-8', newline='').write(s)


edit(CS, [
    # the anchor became AMBIGUOUS: both the `gap` branch and the `pad` branch now
    # set octMipUnit from the gap, which is the point of the change and also why
    # the old anchor can no longer name one line
    ('| the reader takes `gap`, and an older `pad` under its own law | `lodgen.cpp:2494-2513` | `card.octMipUnit = qMin( card.octGapX, card.octGapY );` |',
     '| the reader takes `gap`, and an older `pad` as HALF a gap | `lodgen.cpp:2494-2513` | `card.octPadX = card.octGapX / 2;` |\n'
     '| per-frame offsets read off the sidecar, placed once the grid is known | `lodgen.cpp:2494-2513` | `card.octFrameOff[2 * ( fj * card.oct + fi )] = rawFrameOff[k + 2];` |'),
    ('| `mips = 1 + log2(min(gapX,gapY))` | `lodgen.cpp:2624-2625` |',
     '| `mips = log2(min(gapX,gapY))`, floored at 1 | `lodgen.cpp:2624-2625` |'),
    ('| the card `.lodm` carries `pad` (per side) and `gap` | `lodgen.cpp:2707-2708` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |',
     '| the card `.lodm` carries `pad` (per side) and `gap` | `lodgen.cpp:2707-2708` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |\n'
     '| the card `.lodm` carries `frameOffset`, absent when the bake wrote none | `lodgen.cpp:2707-2708` | `oc.insert( QStringLiteral( "frameOffset" ), fo );` |\n'
     '| a `cardArray` layer carries its own `frameOffset` | `lodgen.cpp:8328-8330` | `o.insert( QStringLiteral( "frameOffset" ), L.frameOff );` |'),
    ('| the sidecar\'s own `gap` line | `nifskope_ui.cpp:22434` | `ms << "gap " << gapX << " " << gapY` |',
     '| the sidecar\'s own `gap` line | `nifskope_ui.cpp:22434` | `ms << "gap " << gapX << " " << gapY` |\n'
     '| the sidecar\'s per-frame offset lines, up-positive | `nifskope_ui.cpp:22434` | `ms << "frameoff " << i << " " << j << " "` |\n'
     '| the frames that had to be clamped, and the fit the gain is read from | `nifskope_ui.cpp:22434` | `ms << "framefit " << maxDx << " " << maxDy << " " << unionDx << " " << unionDy` |'),
    ('| the measurement margin is 1%, not 4% | `nifskope_ui.cpp:22144` | `float halfW = maxDx * 1.01f, halfH = maxDy * 1.01f;` |',
     '| the measurement margin is 1%, not 4% | `nifskope_ui.cpp:22144` | `float halfW = maxDx * 1.01f, halfH = maxDy * 1.01f;` |\n'
     '| the frame is sized from the WIDEST SINGLE view, not the union | `nifskope_ui.cpp:22144` | `maxDx = qMax( maxDx, 0.5f * ( x1 - x0 ) );` |\n'
     '| the size ladder is still fed the UNION half-extent | `nifskope_ui.cpp:22144` | `const float myExtent = qMax( unionDx, unionDy );` |\n'
     '| each channel is cropped around THAT view\'s own centre | `nifskope_ui.cpp:22144` | `const QImage tA = frameOf( matte(), ox, oy );` |\n'
     '| the viewport fit is widened by the largest offset | `nifskope_ui.cpp:22144` | `const float fitH = qMax( maxOffY + halfH, ( maxOffX + halfW ) * viewH / viewW );` |'),
])

edit(LF, [
    # the anchor line itself was rewritten: the array packer no longer branches on
    # which vintage the set is, because the gap is now the unit under all of them
    ('| the three vintages read under their own laws | `lodgen.cpp:8207-8216` | `const int mipUnit = gapA.size() == 2 ? qMin( gapX, gapY ) : qMin( padX, padY );` |',
     '| the three vintages all reduce to a GAP, and the cap divides it | `lodgen.cpp:8207-8216` | `const int mipUnit = qMin( gapX, gapY );` |'),
    ('| `mips` is derived from the gap | `lodgen.cpp:2624-2625` | `for ( int g = mipUnit; g >= 2; g /= 2 )` |',
     '| `mips` is derived from the gap, one level shallower than before | `lodgen.cpp:2624-2625` | `frameMips = qMax( 1, frameMips );` |\n'
     '| `card.frameOffset`, one pair a frame in sheet order | `lodgen.cpp:2707` | `oc.insert( QStringLiteral( "frameOffset" ), fo );` |\n'
     '| a `cardArray` layer\'s own `frameOffset` | `lodgen.cpp:8328` | `o.insert( QStringLiteral( "frameOffset" ), L.frameOff );` |'),
])

# the file hashes and line counts, LAST and read from disk
for doc, files in ((CS, ('src/lodgen.cpp', 'src/nifskope_ui.cpp')), (LF, ())):
    if not files:
        continue
    s = open(doc, encoding='utf-8').read()
    for f in files:
        b = open(f, 'rb').read()
        h = hashlib.sha256(b).hexdigest()[:16]
        n = b.count(b'\n')
        import re
        pat = re.compile(r'\| `%s` \| `[0-9a-f]{16}` \| [\d,]+ \|' % re.escape(f))
        m = pat.search(s)
        if not m:
            print('%s: no hash row for %s' % (doc, f))
            sys.exit(1)
        s = s.replace(m.group(0), '| `%s` | `%s` | %s |' % (f, h, format(n, ',')))
        print('%s -> %s, %s lines' % (f, h, format(n, ',')))
    open(doc, 'w', encoding='utf-8', newline='').write(s)
print('rows and hashes written')
