#!/usr/bin/env python3
"""CARDFIT3 fix 07 -- the provenance footers of the two contract pages, which
went stale the moment this lane edited the sources they cite
(`ww-contract-provenance`: hash and line count first, anchor text beside every
line number, and the source hash re-read end to end).

Read 2026-09-09 21:2x, after the last edit to either file:
  src/lodgen.cpp        1f5084a2c2e33ae9   8,344 lines
  src/nifskope_ui.cpp   9a1fcbf7838a0257  31,110 lines
  src/io/lodmfile.cpp   f3d9a99b7a12677b     115 lines  (untouched by this lane)
"""
ROWS_CARD = """| `oct = N` frames per side, N² views | `nifskope_ui.cpp:21847-21848` | `for ( int j = 0; j < octN; j++ ) { for ( int i = 0; i < octN; i++ )` |
| the padding law `max(2, side/16)` rounded up to even | `nifskope_ui.cpp:22206-22209` | `auto padOf = []( int side ) {`, `return p + ( p & 1 );` |
| the short side, smallest multiple of 16 that does not crop | `nifskope_ui.cpp:22230-22236` | `for ( int s = 16; s <= tileLong; s += 16 ) {` |
| the inner rect is `frame - 2*pad`, per axis | `nifskope_ui.cpp:22242-22243` | `const int padX = padOf( tw ), padY = padOf( th );` |
| the measurement margin is 1%, not 4% | `nifskope_ui.cpp:22139` | `float halfW = maxDx * 1.01f, halfH = maxDy * 1.01f;` |
| the sidecar's own `pad` line | `nifskope_ui.cpp:22411` | `ms << "pad " << padX << " " << padY` |
| `mips = 1 + log2(min(padX,padY))` | `lodgen.cpp:2593-2594` | `for ( int g = qMin( padX, padY ); g >= 2; g /= 2 )` |
| `auxMips` comes down with the halved padding | `lodgen.cpp:2600-2602` | `for ( int g = qMin( padX, padY ) / auxDiv; g >= 2; g /= 2 )` |
| the card `.lodm` carries `pad` | `lodgen.cpp:2668` | `oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );` |
| the `cardArray` `.lodm` carries `pad` | `lodgen.cpp:8278` | `arr.insert( QStringLiteral( "pad" ), QJsonArray{ g.padX, g.padY } );` |
| `_fs.DDS` is BC3/DXT5 with its alpha | `lodgen.cpp:2529` | `lodgenWriteDds( dds, 2 * w, h, px, true );` |
| per-card game path stem | `lodgen.cpp:2596` | `QStringLiteral( "Data\\\\Textures\\\\Lodgen\\\\Cards\\\\" ) + id + QStringLiteral( "_oct" )` |
| emissive sheet written BC1, no alpha | `lodgen.cpp:2612-2616` | `// the emissive sheet is BC1: RGB only, no alpha to carry` |
| dilation depth `max(8, max(fw,fh)/8)` | `lodgen.cpp:8177` | `const int deep = qMax( 8, qMax( fw, fh ) / 8 );` |
| card-array group key = family + sheet size | `lodgen.cpp:8188` | `const QString key = QString( "%1\\|%2x%3" )` |
| DX10 array header fields | `lodgen.cpp:3975-4005` | `const quint32 dx10[5] = { bc3 ? 77U : 71U, 3U, 0U, quint32( layers.size() ), 0U };` |
| mip filter box + round-half-up | `lodgen.cpp:3898, 3913-3918` | `while ( mw > 4 && mh > 4 && ( maxMips <= 0 \\|\\| … ) )`, `( acc[0] + 2 ) >> 2` |
"""

ROWS_LODM = """| magic, envelope version, 4 MiB cap | `lodmfile.cpp:8-10` | `LODM_MAGIC`, `LODM_VERSION`, `LODM_PAYLOAD_CAP` |
| 12-byte envelope, exact payload size | `lodmfile.cpp:16-38` | `bytes.size() < 12`, `declared != bytes.size() - 12` |
| `lodm != 1` refusal | `lodmfile.cpp:46` | `payload is not a lodm 1 object` |
| family hard refusal | `lodmfile.cpp:51-53` | `family must be legacy or pbr` |
| `kind` defaults to `source` | `lodmfile.cpp:56` | `.toString( QStringLiteral( "source" ) )` |
| `emissiveScale` defaults to 1 | `lodmfile.cpp:63` | `.toDouble( 1.0 )` |
| family-dependent key and suffix names | `lodmfile.h:113-118` | `lodmColorKey`, `lodmMaskKey`, `lodmColorSuffix` |
| `lodmSourceCandidate` two-branch rule | `lodmfile.cpp:84-106` | `c.prepend( QStringLiteral( "materials\\\\" ) )` |
| `kind: "card"` key set | `lodgen.cpp:2646-2680` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) )` |
| `card.pad`, per axis, in texels a side | `lodgen.cpp:2668` | `oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );` |
| `card.center` is the bake's own look-at point | `lodgen.cpp:2670` | `oc.insert( QStringLiteral( "center" ), …card.octCenter…` |
| that point is the scene's bound centre in MODEL space | `nifskope_ui.cpp:22030-22032`, `22409` | `bs = sc->bounds();`, `<< bs.center[0] << " " << bs.center[1]` |
| the renderer recomputes a shape's bound FROM VERTICES | `gl/glmesh.cpp:732` | `boundSphere = BoundSphere( verts );` |
| `mips` is derived from the padding | `lodgen.cpp:2593-2594` | `for ( int g = qMin( padX, padY ); g >= 2; g /= 2 )` |
| card `auxDiv` written only above 1 | `lodgen.cpp:2664-2665` | `if ( auxDiv > 1 )` |
| `kind: "array"` key set, no aux keys | `lodgen.cpp:4291-4310` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "array" ) )` |
| `kind: "cardArray"` key set incl. `pad` | `lodgen.cpp:8255-8300` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) )`, `arr.insert( QStringLiteral( "pad" )` |
| `kind: "terrainVT"` payload | `lodgen.cpp:6805` | `QStringLiteral( "terrainVT" )` |
| the `u = i/(N−1)·2 − 1` mapping | `nifskope_ui.cpp:21789-21791` | `auto viewDir = [octN]` |
"""


def redo(path, header, rows):
    b = open(path, 'rb').read()
    assert b.count(b'\r') == 0, path
    s = b.decode('utf-8')
    i = s.index('\n## Provenance\n')
    s = s[:i] + '\n## Provenance\n\n' + header + '\n' + \
        '| claim | line | anchor |\n|---|---|---|\n' + rows
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(path, 'wb').write(out)
    print('%s: provenance rewritten, %d bytes' % (path, len(out)))


HDR_CARD = """Re-read 2026-09-09 after lane CARDFIT3 changed the frame law. Anchor text is
quoted beside every line number because both sources move.

| file | sha256 (16) | lines |
|---|---|---|
| `src/lodgen.cpp` | `1f5084a2c2e33ae9` | 8,344 |
| `src/nifskope_ui.cpp` | `9a1fcbf7838a0257` | 31,110 |
"""

HDR_LODM = """Re-read 2026-09-09 after lane CARDFIT3 added `card.pad` / `array.pad` and
documented `card.center`. Anchor text is quoted beside every line number.

| file | sha256 (16) | lines |
|---|---|---|
| `src/io/lodmfile.cpp` | `f3d9a99b7a12677b` | 115 |
| `src/lodgen.cpp` | `1f5084a2c2e33ae9` | 8,344 |
| `src/nifskope_ui.cpp` | `9a1fcbf7838a0257` | 31,110 |
"""

redo('docs/LODGEN_CARD_SHEETS.md', HDR_CARD, ROWS_CARD)
redo('docs/LODGEN_LODM_FORMAT.md', HDR_LODM, ROWS_LODM)
