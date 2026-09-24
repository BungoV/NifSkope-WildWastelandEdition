#!/usr/bin/env python
"""CLAMP2 patch 3 -- docs/LODGEN_TERRAIN_VT.md and tests/spells/lodgen_terrain_vt.sh.

The doc gains the seam-ownership rule in 2.4 and its provenance footer is
re-derived (p2_anchors.py, from the anchors, never by adding a delta).
The harness gains a comment only: its check COUNT is pre-registered at 35 and
this lane does not move it.
"""
import io, hashlib

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

# ---------------------------------------------------------------- the document
P = ROOT + 'docs/LODGEN_TERRAIN_VT.md'
s = io.open(P, encoding='utf-8', newline='').read()
assert s.count('\r') == 0, 'the doc is LF-only'

OLD = '''**One sheet is still not identical between the two paths, and it is named**:'''
NEW = '''**Where the two neighbours disagree, the cell wins** (bungo, 2026-09-10,
verbatim: "The cell owns it then"). Bethesda's landscape does not always agree
with itself across a shared cell edge: over the cells x = -24..-17 the shared
VHGT row y=31|32 differs by 2, 1, 4, 6, 9, 8, 7 and 4 units of 8 -- 16 to 72
world units -- while y=23|24, y=27|28, y=32|33 and both east seams differ by
exactly 0. `lodgenTerrainFillRing` therefore fills ONLY the samples BEYOND its
inner unit: a ring cell never writes the chunk's (or the tile's) own boundary
row or column, so the chunk's copy of a disagreeing row stays, and the hairline
disagreement is kept AT the seam instead of being carried inwards. A bilinear
tap reaches one grid step, so before this rule a moved boundary row showed up to
**7 texels** inside the sheet -- past the 4-texel band the normal's own central
difference can reach. Inside the inner unit the fill order is unchanged (south
to north, west to east, later wins), which is also the mesh path's convention in
`lodgenWriteLandChunk`, so the two stay consistent. Both bakers get the rule
from the one shared filler, which is what keeps V9a and V9b byte-identical.

**One sheet is still not identical between the two paths, and it is named**:'''
assert s.count(OLD) == 1
s = s.replace(OLD, NEW)

ROWS = [
    ('`lodgen.cpp:7010-7092`', '`lodgen.cpp:7267-7349`'),
    ('`lodgen.cpp:7024`', '`lodgen.cpp:7281`'),
    ('`lodgen.cpp:7064, 7069`', '`lodgen.cpp:7321, 7326`'),
    ('`lodgen.cpp:7071`', '`lodgen.cpp:7328`'),
    ('`lodgen.cpp:7081-7083`', '`lodgen.cpp:7338-7340`'),
]
for a, b in ROWS:
    assert s.count(a) == 1, 'row anchor %s count %d' % (a, s.count(a))
    s = s.replace(a, b)

src = open(ROOT + 'src/lodgen.cpp', 'rb').read()
STAMP_OLD = '| `src/lodgen.cpp` | `36e00f03dc138693` | 373,908 | 8,574 |'
STAMP_NEW = ('| `src/lodgen.cpp` | `%s` | %s | %s |'
             % (hashlib.sha256(src).hexdigest()[:16],
                '{:,}'.format(len(src)), '{:,}'.format(src.count(b'\n'))))
assert s.count(STAMP_OLD) == 1
s = s.replace(STAMP_OLD, STAMP_NEW)

PROV_OLD = '''Section 3 (the container) was re-read against the writer on 2026-09-09; every
offset, stride, flag value and refusal below matched the document as it already
stood. Sections 1, 2 and 4 are bake laws rather than byte layout and are traced
to `src/lodgen.cpp`.'''
PROV_NEW = '''Section 3 (the container) was re-read against the writer on 2026-09-09; every
offset, stride, flag value and refusal below matched the document as it already
stood. Sections 1, 2 and 4 are bake laws rather than byte layout and are traced
to `src/lodgen.cpp`.

The five `src/lodgen.cpp` rows below were **re-derived from their own anchors**
on 2026-09-10 (`scratchpad/clamp2_20260910/p2_anchors.py`), not shifted by this
lane's own insertion: two lanes moved that file since the stamp was taken, and
the file the footer named at 373,908 bytes was already 380,327 before lane
CLAMP2 touched it. Every anchor was found exactly once; all five rows moved by
the same +257, which is the arithmetic cross-check on the re-derivation and not
its method.'''
assert s.count(PROV_OLD) == 1
s = s.replace(PROV_OLD, PROV_NEW)

io.open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('doc: CR %d LF %d bytes %d' % (b.count(b'\r'), b.count(b'\n'), len(b)))
print('doc: new stamp row -> %s' % STAMP_NEW)

# ---------------------------------------------------------------- the harness
H = ROOT + 'tests/spells/lodgen_terrain_vt.sh'
t = io.open(H, encoding='utf-8', newline='').read()
assert t.count('\r') == 0, 'the harness is LF-only'
HOLD = '''\t# V9b: the msn is the OPERAND that used to differ (5,524 texels on'''
HNEW = '''\t# The two paths agree here because ONE filler serves both, and since
\t# 2026-09-10 that filler also decides who owns a DISAGREEING shared row: the
\t# cell does. Bethesda's y=31|32 row differs by up to 9 VHGT units in this very
\t# fixture, and the old south-to-north order carried the neighbour's copy 7
\t# texels into the chunk. The exact known-answer control for that rule is
\t# scratchpad/clamp2_20260910/ringcontrol.sh (a synthetic pair, the OLD order as
\t# the refuter); it is deliberately NOT a check here, because this file's count
\t# is pre-registered at 35 and a lane does not grow the number it is judged by.
\t# V9b: the msn is the OPERAND that used to differ (5,524 texels on'''
assert t.count(HOLD) == 1
t = t.replace(HOLD, HNEW)
io.open(H, 'w', encoding='utf-8', newline='').write(t)
b = open(H, 'rb').read()
print('harness: CR %d LF %d bytes %d' % (b.count(b'\r'), b.count(b'\n'), len(b)))
