#!/usr/bin/env python
"""CLAMP2 -- re-derive docs/LODGEN_TERRAIN_VT.md's src/lodgen.cpp line numbers
from their OWN anchors (ww-contract-provenance step 3), never by adding the
insertion's delta: two other lanes moved that file since the footer was written.

Prints, does not write. The numbers go into p3_doc.py by hand after reading.
"""
import io, hashlib

SRC = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
lines = io.open(SRC, encoding='utf-8', newline='').read().split('\n')
b = open(SRC, 'rb').read()
print('sha256(16) %s  bytes %d  lines %d'
      % (hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b'\n')))

ANCHORS = [
    ('kind terrainVT (range START)',
     'root.insert( QStringLiteral( "kind" ), QStringLiteral( "terrainVT" ) );'),
    ('aniso',
     't.insert( QStringLiteral( "aniso" ), qMin( 16, 2 * ( border >> ( mips - 1 ) ) ) );'),
    ('coarseLevelsAreDownsamples',
     't.insert( QStringLiteral( "coarseLevelsAreDownsamples" ), true );'),
    ('alignedToWorldOrigin',
     'alignedToWorldOrigin'),
    ('partial true',
     't.insert( QStringLiteral( "partial" ), true );'),
    ('worldUnitsPerTile (range START)',
     'o.insert( QStringLiteral( "worldUnitsPerTile" ), levels[l].dim * 4096 );'),
    ('unitsPerTexel (range END)',
     'o.insert( QStringLiteral( "unitsPerTexel" ),'),
]
for name, a in ANCHORS:
    hits = [i + 1 for i, l in enumerate(lines) if a in l]
    print('%-34s %s' % (name, hits if hits else 'MISSING'))

# the §4 range's END: the writer block's closing, found from its start
start = [i for i, l in enumerate(lines)
         if 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "terrainVT" ) );' in l]
if start:
    s = start[0]
    for i in range(s, min(s + 200, len(lines))):
        if lines[i].startswith('\t}') and 'levels' not in lines[i]:
            pass
    print('range start line %d; next 6 lines after unitsPerTexel shown below' % (s + 1))
    ut = [i for i, l in enumerate(lines) if 'o.insert( QStringLiteral( "unitsPerTexel" ),' in l]
    if ut:
        for i in range(ut[0], ut[0] + 12):
            print('  %5d %s' % (i + 1, lines[i]))
