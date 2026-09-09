#!/usr/bin/env python3
"""CARDPAD -- the provenance rows of the two contract pages: re-anchor every
claim this lane moved. Line NUMBERS are re-derived afterwards by the anchor
pass (p14_anchors adapted), and the hash/line table last of all."""

def edit(path, pairs):
    b = open(path, 'rb').read()
    cr0 = b.count(b'\r')
    s = b.decode('utf-8')
    for what, old, new in pairs:
        n = s.count(old)
        assert n == 1, '%s / %s: %d' % (path, what, n)
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0
    open(path, 'wb').write(nb)
    print('%s: %d -> %d bytes' % (path, len(b), len(nb)))


CS = [
 ('header',
  """Re-read 2026-09-09 after lane CARDFIT3 changed the frame law. Anchor text is
quoted beside every line number because both sources move.""",
  """Re-read 2026-09-09 after lane CARDPAD moved the spacing from a per-side margin
to the GAP between two neighbouring silhouettes (bungo's correction of lane
CARDFIT3 the same day). Anchor text is quoted beside every line number because
both sources move."""),

 ('pad law row',
  """| the padding law `max(2, side/16)` rounded up to even | `nifskope_ui.cpp:22206-22209` | `auto padOf = []( int side ) {`, `return p + ( p & 1 );` |""",
  """| the gap law `max(2, side/16)` rounded up to even | `nifskope_ui.cpp:22206-22209` | `auto gapOf = []( int side ) {` |
| the margin on each side is half the gap | `nifskope_ui.cpp:22210` | `auto padOf = [gapOf]( int side ) { return gapOf( side ) / 2; };` |"""),

 ('inner rect row',
  """| the inner rect is `frame - 2*pad`, per axis | `nifskope_ui.cpp:22242-22243` | `const int padX = padOf( tw ), padY = padOf( th );` |""",
  """| the inner rect is `frame - 2*pad` = `frame - gap`, per axis | `nifskope_ui.cpp:22242-22243` | `const int padX = padOf( tw ), padY = padOf( th );` |
| the gap the sidecar records | `nifskope_ui.cpp:22244` | `const int gapX = gapOf( tw ), gapY = gapOf( th );` |"""),

 ('meta line row',
  """| the sidecar's own `pad` line | `nifskope_ui.cpp:22411` | `ms << "pad " << padX << " " << padY` |""",
  """| the sidecar's own `gap` line | `nifskope_ui.cpp:22411` | `ms << "gap " << gapX << " " << gapY` |
| the reader takes `gap`, and an older `pad` under its own law | `lodgen.cpp:2477-2496` | `card.octMipUnit = qMin( card.octGapX, card.octGapY );` |"""),

 ('mips row',
  """| `mips = 1 + log2(min(padX,padY))` | `lodgen.cpp:2593-2594` | `for ( int g = qMin( padX, padY ); g >= 2; g /= 2 )` |
| `auxMips` comes down with the halved padding | `lodgen.cpp:2600-2602` | `for ( int g = qMin( padX, padY ) / auxDiv; g >= 2; g /= 2 )` |""",
  """| `mips = 1 + log2(min(gapX,gapY))` | `lodgen.cpp:2593-2594` | `for ( int g = mipUnit; g >= 2; g /= 2 )` |
| `auxMips` comes down with the halved gap | `lodgen.cpp:2600-2602` | `for ( int g = mipUnit / auxDiv; g >= 2; g /= 2 )` |"""),

 ('lodm rows',
  """| the card `.lodm` carries `pad` | `lodgen.cpp:2668` | `oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );` |
| the `cardArray` `.lodm` carries `pad` | `lodgen.cpp:8278` | `arr.insert( QStringLiteral( "pad" ), QJsonArray{ g.padX, g.padY } );` |""",
  """| the card `.lodm` carries `pad` (per side) and `gap` | `lodgen.cpp:2668-2669` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |
| the `cardArray` `.lodm` carries `pad` and `gap` | `lodgen.cpp:8278-8280` | `arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );` |"""),
]
edit('docs/LODGEN_CARD_SHEETS.md', CS)


LM = [
 ('header',
  """Re-read 2026-09-09 after lane CARDFIT3 added `card.pad` / `array.pad` and
documented `card.center`. Anchor text is quoted beside every line number.""",
  """Re-read 2026-09-09 after lane CARDPAD added `card.gap` / `array.gap` and made
`card.pad` the PER-SIDE half of it (bungo's correction of lane CARDFIT3 the same
day). Anchor text is quoted beside every line number."""),

 ('card pad row',
  """| `card.pad`, per axis, in texels a side | `lodgen.cpp:2668` | `oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );` |""",
  """| `card.pad`, per axis, in texels a side | `lodgen.cpp:2668` | `oc.insert( QStringLiteral( "pad" ), QJsonArray{ padX, padY } );` |
| `card.gap`, per axis, twice the padding | `lodgen.cpp:2669` | `oc.insert( QStringLiteral( "gap" ), QJsonArray{ gapX, gapY } );` |
| the three vintages read under their own laws | `lodgen.cpp:8159-8168` | `const int mipUnit = gapA.size() == 2 ? qMin( gapX, gapY ) : qMin( padX, padY );` |"""),

 ('mips row',
  """| `mips` is derived from the padding | `lodgen.cpp:2593-2594` | `for ( int g = qMin( padX, padY ); g >= 2; g /= 2 )` |""",
  """| `mips` is derived from the gap | `lodgen.cpp:2593-2594` | `for ( int g = mipUnit; g >= 2; g /= 2 )` |"""),

 ('cardArray row',
  """| `kind: "cardArray"` key set incl. `pad` | `lodgen.cpp:8255-8300` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) )`, `arr.insert( QStringLiteral( "pad" )` |""",
  """| `kind: "cardArray"` key set incl. `pad` and `gap` | `lodgen.cpp:8255-8300` | `root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) )` |
| `array.gap`, shared | `lodgen.cpp:8280` | `arr.insert( QStringLiteral( "gap" ), QJsonArray{ g.gapX, g.gapY } );` |"""),
]
edit('docs/LODGEN_LODM_FORMAT.md', LM)
