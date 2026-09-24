"""CARDLINK1 patch 1: src/nativeemit.h + src/nativeemit.cpp. Anchors asserted once, CR count kept."""
import os, sys
WT = 'E:/Projects/NifskopeWWE-cardlink1'
P = WT + '/scratchpad/cardlink1_20260924/patch/'

def snip(n):
    with open(P + n, 'rb') as f:
        return f.read().replace(b'\r\n', b'\n')

def patch(path, reps):
    with open(path, 'rb') as f:
        b = f.read()
    cr0 = b.count(b'\r')
    for old, new in reps:
        c = b.count(old)
        if c != 1:
            sys.exit('%s: anchor count %d for %r' % (path, c, old[:80]))
        b = b.replace(old, new)
    if b.count(b'\r') != cr0:
        sys.exit('%s: CR count moved' % path)
    with open(path, 'wb') as f:
        f.write(b)
    print('patched', path, len(reps), 'anchors')

patch(WT + '/src/nativeemit.h', [
    (b'void lodgenNativeOfferLibraryReuse( const NativeReuseOffer & offer );\n', snip('s_decl.txt')),
])

cpp = WT + '/src/nativeemit.cpp'
patch(cpp, [
    (b'#include "lodgenao.h"\n', b'#include "lodgenao.h"\n#include "io/lodmfile.h"\n'),
    (b'#include <QHash>\n', b'#include <QHash>\n#include <QJsonArray>\n#include <QJsonObject>\n'),
    (b'#include <map>\n', b'#include <map>\n#include <set>\n'),
    (b'\tLodgenAggStats aggStats;\n};\n', snip('s_state.txt')),
    (b'void lodgenNativeEnd()\n{\n', snip('s_link.txt')),
    (b'\tint basesWritten = 0, basesWithoutMesh = 0;\n',
     b'\tint basesWritten = 0, basesWithoutMesh = 0;\n\tint cardOnlyBases = 0;     //!< CARDLINK1: bases written with a card and no mesh\n'),
    (b'\t\telse\n\t\t\tlibraryReused = true;\n',
     b'\t\telse if ( lh.cardCorpusHash != ( s.cardsLinked ? s.cardHash : 0 ) )\n'
     b'\t\t\tlibraryWhy = QStringLiteral( "the card arrays moved" );\n'
     b'\t\telse\n\t\t\tlibraryReused = true;\n'),
    (b'\tlib.cardCorpusHash = 0;\n',
     b'\t// CARDLINK1: the proposed R19 hash over the linked arrays, 0 when none were linked\n'
     b'\tlib.cardCorpusHash = s.cardsLinked ? s.cardHash : 0;\n'),
    (b'\t\t\trow.cardLayer = LODO_NO_CARD;\n',
     b'\t\t\trow.cardLayer = s.cardLayerOf.value( b, LODO_NO_CARD );\n'),
    (b'\t\t\tif ( !any || !( radius > 0.0f ) ) {\n'
     b'\t\t\t\tbasesWithoutMesh++;         // no slot loaded: the stock bake draws nothing for it either\n'
     b'\t\t\t\tcontinue;\n'
     b'\t\t\t}\n'
     b'\t\t\trow.flags = quint16( ( tree ? LODO_BASE_TREE : 0 ) | ( alpha ? LODO_BASE_ANY_ALPHA : 0 ) | LODO_BASE_ANY_MESH );\n',
     snip('s_baseloop.txt')),
    (b'\tquint32 scrappablePlacements = 0;\n',
     b'\tquint32 scrappablePlacements = 0;\n'
     b'\t//! CARDLINK1 census: instances given FORCE_CARD, and which clause gave it\n'
     b'\tquint32 forcedCard = 0, forcedEmptySlot = 0, forcedByLine = 0;\n'),
    (b'\t\t\t| ( p.emits ? LODI_INST_EMITS : 0 ) | ( p.scolPart >= 0 ? LODI_INST_SCOL_PART : 0 ) );\n',
     snip('s_force.txt')),
    (b'\t\t/* THE PER-SOURCE CASTER COUNTS, on their own prefix.',
     snip('s_census.txt') + b'\t\t/* THE PER-SOURCE CASTER COUNTS, on their own prefix.'),
])
