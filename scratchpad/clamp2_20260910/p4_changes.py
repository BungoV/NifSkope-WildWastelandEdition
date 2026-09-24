#!/usr/bin/env python
"""CLAMP2 patch 4 -- the WW_CHANGES.md entry.

WW_CHANGES.md is MIXED (19,020 CR / 24,368 LF): the newest entries at the top of
the file are LF, so the insert is LF and the CR count must not move by one.
Measured with Python byte counts before and after, never grep.
"""
import io

P = 'E:/Projects/NifskopeWildWastelandEdition/WW_CHANGES.md'
b = open(P, 'rb').read()
cr0, lf0 = b.count(b'\r'), b.count(b'\n')

TITLE = b'# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n'
assert b.startswith(TITLE), 'the title block is not where it was'

ENTRY = u'''## 2026-09-10 - the cell owns its own boundary rows

`src/lodgen.cpp` (`lodgenTerrainFillRing` and a new
`lodgenTerrainRingSelfTest`), `docs/LODGEN_TERRAIN_VT.md` (\u00a72.4 and the
provenance footer), `tests/spells/lodgen_terrain_vt.sh` (a comment; the check
count stays 35), `scratchpad/clamp2_20260910/ringcontrol.sh`.

**NOT BUILT AND NOT GATED YET** -- the code, the control and this entry are on
disk; the resume is `scratchpad/clamp2_20260910/PENDING.md` and it carries the
gate table. No number below is a reading off a built exe; the seam measurements
are lane BUILD4's, off the master.

Bethesda's landscape does not always agree with itself across a shared cell
edge. Over the cells x = -24..-17 the shared VHGT row **y=31|32 differs by 2, 1,
4, 6, 9, 8, 7 and 4 units of 8** -- 16 to 72 world units -- while y=23|24,
y=27|28, y=32|33 and both east seams differ by exactly 0 (lane BUILD4, from
`--dump-land`, not from our output). The one-cell ring the terrain bake got on
2026-09-10 filled south to north and west to east with later-wins, so the y=32
neighbour rewrote the row it shares with y=31 -- **which is the chunk's own
northern boundary row** -- and a bilinear tap carried that 7 texels into the
sheet, past the 4-texel band the normal's central difference can reach. The
`_msn` sheets of the two y=28 fixture chunks moved 994 and 1,405 texels beyond
that band, every one of them on the NORTH border, at distances 4, 5, 6 and 7.

bungo's ruling, verbatim: **"The cell owns it then."** `lodgenTerrainFillRing`
now fills ONLY the samples BEYOND its inner unit: a ring cell never writes the
inner unit's own boundary row or column, so the cell's copy of a disagreeing
shared row stays and the hairline disagreement is kept AT the seam rather than
smeared inwards. The rule is derived inside the one shared filler from
`LODGEN_TERRAIN_RING_CELLS`, so the chunk baker and `lodgenBakeVtTile` get it
from the same seven lines and the V9a/V9b byte-identity bars still stand on one
implementation. Inside the inner unit the order is unchanged -- south to north,
west to east, later wins -- which is also the mesh path's convention, and the
south and west borders were already the cell's own, so only the north row and
the east column move.

**The land VERTEX channels do not need the ring for this rule and are
unchanged** (`src/lodgen.cpp:903`, `lodgenTerrainChannels` on the mesh path):
its `grid` is built over the chunk's own `dim` cells only, `n = dim*32+1`, with
no ring at all, so every boundary sample it holds was written by the chunk's own
cell and the cell already owned it. What that grid still lacks is the ring
itself -- its AO march is clamped at the chunk edge, which is a different defect,
named as owed and untouched here because it moves `.bto`/`.btr` bytes and
`lodgen_identity.sh`'s baseline.

**The known-answer control**, `scratchpad/clamp2_20260910/ringcontrol.sh` on the
exe's own `WW_TERRAIN_RING_TEST` self-test: a synthetic pair of cells whose
shared row disagrees by 72 world units -- the measured 9-unit worst case -- is
filled through the shipped filler, and every sample of all four inner boundary
rows/columns must equal the inner cell's value EXACTLY while the samples one
step beyond must equal the neighbour's, so a filler that stopped ringing fails
too. The bilinear tap is asserted at the same place, and half a step beyond it,
so the claim is about a texel's operand. **The refuter is the OLD fill order**,
reproduced verbatim beside it: it must give the neighbour's value on the inner
boundary, and if it does not the self-test fails on that alone.
'''

out = TITLE + ENTRY.encode('utf-8') + b'\n' + b[len(TITLE):]
open(P, 'wb').write(out)
b2 = open(P, 'rb').read()
print('WW_CHANGES.md CR %d -> %d (dCR %d), LF %d -> %d (+%d), bytes %d'
      % (cr0, b2.count(b'\r'), b2.count(b'\r') - cr0, lf0, b2.count(b'\n'),
         b2.count(b'\n') - lf0, len(b2)))
assert b2.count(b'\r') == cr0, 'the CR count moved'
