#!/usr/bin/env python3
"""CARDPAD -- repair the provenance anchors the automatic pass could not resolve.

Four were broken before this lane (a two-line anchor no single line can hold, two
that match three places each, one carrying an ellipsis inside its own backticks)
and three are multi-site.  Each is re-anchored on a fragment that is UNIQUE in
the current source -- asserted here -- so the scripted pass can keep the numbers
honest from now on."""

SRC = {}
for f in ('src/lodgen.cpp', 'src/nifskope_ui.cpp', 'src/io/lodmfile.cpp'):
    SRC[f.split('/')[-1]] = open(f, encoding='utf-8').read().split('\n')


def uniq(base, frag):
    hits = [i for i, ln in enumerate(SRC[base], 1) if frag in ln]
    assert len(hits) == 1, '%s %r -> %s' % (base, frag[:60], hits)
    return hits[0]


def edit(path, pairs):
    b = open(path, 'rb').read()
    cr0 = b.count(b'\r')
    s = b.decode('utf-8')
    for what, old, new in pairs:
        assert s.count(old) == 1, '%s / %s: %d' % (path, what, s.count(old))
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0
    open(path, 'wb').write(nb)
    print('%s: %d -> %d bytes' % (path, len(b), len(nb)))


A_OCT = 'auto viewDir = [octN]( int i, int j, float & rx, float & rz ) {'
A_DEEP1 = 'const int deep = qMax( 8, qMax( card.octTileW, card.octTileH ) / 8 );'
A_DEEP2 = 'const int deep = qMax( 8, qMax( fw, fh ) / 8 );'
A_MIP = '// texels, or neighbouring views blend into one another.'
A_MIP2 = 'std::vector<quint8> & out, int maxMips = 0 )'
A_CENTER = 'oc.insert( QStringLiteral( "center" ), QJsonArray{ double( card.octCenter[0] )'
A_AUX = 'oc.insert( QStringLiteral( "auxDiv" ), auxDiv );'
A_CAP = 'static const qsizetype LODM_PAYLOAD_CAP'
A_BOUND = '<< bs.center[0] << " " << bs.center[1] << " " << bs.center[2] << " " << depthSpan'

L_OCT = uniq('nifskope_ui.cpp', A_OCT)
L_DEEP1 = uniq('lodgen.cpp', A_DEEP1)
L_DEEP2 = uniq('lodgen.cpp', A_DEEP2)
L_MIP = uniq('lodgen.cpp', A_MIP)
L_MIP2 = uniq('lodgen.cpp', A_MIP2)
L_CENTER = uniq('lodgen.cpp', A_CENTER)
L_AUX = uniq('lodgen.cpp', A_AUX)
L_CAP = uniq('lodmfile.cpp', A_CAP)
L_BOUND = uniq('nifskope_ui.cpp', A_BOUND)
print('resolved: viewDir %d, deep %d/%d, mipfilter %d, center %d, auxDiv %d, cap %d, bound %d'
      % (L_OCT, L_DEEP1, L_DEEP2, L_MIP, L_CENTER, L_AUX, L_CAP, L_BOUND))

edit('docs/LODGEN_CARD_SHEETS.md', [
 ('octN row',
  '| `oct = N` frames per side, N² views | `nifskope_ui.cpp:21847-21848` | `for ( int j = 0; j < octN; j++ ) { for ( int i = 0; i < octN; i++ )` |',
  '| `oct = N` frames per side, N² views | `nifskope_ui.cpp:%d` | `auto viewDir = [octN]( int i, int j, float & rx, float & rz ) {` |' % L_OCT),
 ('dilation depth row',
  '| dilation depth `max(8, max(fw,fh)/8)` | `lodgen.cpp:8188` (arrays), `2568` (per card) | `const int deep = qMax( 8, qMax( fw, fh ) / 8 );` |',
  '| dilation depth `max(8, max(fw,fh)/8)`, per card | `lodgen.cpp:%d` | `const int deep = qMax( 8, qMax( card.octTileW, card.octTileH ) / 8 );` |\n'
  '| the same depth in the card-array path | `lodgen.cpp:%d` | `const int deep = qMax( 8, qMax( fw, fh ) / 8 );` |' % (L_DEEP1, L_DEEP2)),
 ('mip filter row',
  '| mip filter box + round-half-up | `lodgen.cpp:3898, 3913-3918` | `while ( mw > 4 && mh > 4 && ( maxMips <= 0 \\|\\| … ) )`, `( acc[0] + 2 ) >> 2` |',
  '| mip filter box + round-half-up, single sheet | `lodgen.cpp:%d` | `// texels, or neighbouring views blend into one another.` |' % L_MIP
  + '\n| the same filter in the ARRAY writer | `lodgen.cpp:%d` | `std::vector<quint8> & out, int maxMips = 0 )` |' % L_MIP2),
])

edit('docs/LODGEN_LODM_FORMAT.md', [
 ('cap row',
  '| magic, envelope version, 4 MiB cap | `lodmfile.cpp:8-10` | `LODM_MAGIC`, `LODM_VERSION`, `LODM_PAYLOAD_CAP` |',
  '| magic, envelope version, 4 MiB cap | `lodmfile.cpp:%d` | `static const qsizetype LODM_PAYLOAD_CAP` |' % L_CAP),
 ('center row',
  '| `card.center` is the bake\'s own look-at point | `lodgen.cpp:2670` | `oc.insert( QStringLiteral( "center" ), …card.octCenter…` |',
  '| `card.center` is the bake\'s own look-at point | `lodgen.cpp:%d` | `oc.insert( QStringLiteral( "center" ), QJsonArray{ double( card.octCenter[0] )` |' % L_CENTER),
 ('bound row',
  '| that point is the scene\'s bound centre in MODEL space | `nifskope_ui.cpp:22054`, `22419` | `bs = sc->bounds();`, `<< bs.center[0] << " " << bs.center[1] << " " << bs.center[2]` |',
  '| that point is the scene\'s bound centre in MODEL space | `nifskope_ui.cpp:%d` | `<< bs.center[0] << " " << bs.center[1] << " " << bs.center[2] << " " << depthSpan` |' % L_BOUND),
 ('auxDiv row',
  '| card `auxDiv` written only above 1 | `lodgen.cpp:2664-2665` | `if ( auxDiv > 1 )` |',
  '| card `auxDiv` written only above 1 | `lodgen.cpp:%d` | `oc.insert( QStringLiteral( "auxDiv" ), auxDiv );` |' % L_AUX),
])
