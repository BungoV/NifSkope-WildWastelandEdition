"""BLENDSEAM1: the doc's chunk-edge limitation is retired; the V9a comment says why V9a-1/-2 are green.

Anchored, exact-once, CR count preserved per file. --check writes nothing."""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
CHECK = '--check' in sys.argv

FILES = {
    'docs/LODGEN_TERRAIN_VT.md': [
        ("""**Limitation, by name:** a quadrant border lying on the chunk's own edge is not
blended, because the neighbouring cell's paint is not loaded there; the adjacent
chunk does not blend it from its side either, so no NEW seam is made, but that
one line stays as hard as it is today. All seven interior lines per axis are
blended.
""",
         """**The chunk's own edge is blended too** (lane BLENDSEAM1, 2026-09-23). A
quadrant line on the chunk edge is a quadrant line like the other seven: both
writers read the neighbouring chunk's paint out of the same one-cell ring the
height grid already carries, so each side of a chunk boundary meets the other on
the same 50/50 mix. Only a cell with no LAND falls back to the quadrant's own
colour. Until that day this paragraph named the chunk edge as a limitation, and
it was true of the STOCK writer only: the pyramid -- whose sheet ships -- had
always blended it, so with the blend on the two writers disagreed on every
texel within the margin of the chunk edge (6,718 of 1,048,576 over the four
V9a chunks, max 25 levels, all within 3 px of the edge) and V9a-1/-2 were red.
Measured on those four chunks, the step across the two internal chunk
boundaries (boundary step over mean step, x / y): blend off 1.510 / 1.633, the
old stock blend 1.513 / 1.644 (it did nothing there), the pyramid and now both
writers 1.340 / 1.431. The stock writer keeps the ring's paint in its own array,
filled only with the blend on, so the dominant base, the cover constants and
`--blend-edges off` are what they were.
"""),
    ],
    'tests/spells/lodgen_terrain_vt.sh': [
        ("""# the four land switches AND the hard quadrant lines. The shipped-default pair
# (V9a-1/-2) is NOT re-rung: with the blend on, the two colour writers disagree
# (DEFAULTS2 measured 2,790 of 262,144 texels on chunk 4.-20.24, all within 4 px
# of a quadrant line and 8 px of the chunk edge, max 25 levels) and it stays red.
""",
         """# the four land switches AND the hard quadrant lines. The shipped-default pair
# (V9a-1/-2) was NOT re-rung: with the blend on, the two colour writers disagreed
# (DEFAULTS2 measured 2,790 of 262,144 texels on chunk 4.-20.24, all within 4 px
# of a quadrant line and 8 px of the chunk edge, max 25 levels) and it stayed red.
# GREEN AGAIN at the same bar (lane BLENDSEAM1, 2026-09-23): the stock writer fell
# back to its own colour at the chunk edge where the pyramid cross-faded into the
# next chunk's paint; the stock writer now reads the same one-cell ring's paint.
"""),
    ],
}

ok = True
for f, eds in FILES.items():
    s = open(ROOT + f, 'rb').read().decode('utf-8')
    for i, (a, _) in enumerate(eds):
        n = s.count(a)
        print('%s edit %d: anchor count %d' % (f, i + 1, n))
        ok &= (n == 1)
if not ok:
    print('REFUSED')
    sys.exit(1)
if CHECK:
    print('check only')
    sys.exit(0)
for f, eds in FILES.items():
    b = open(ROOT + f, 'rb').read()
    cr0 = b.count(b'\r')
    s = b.decode('utf-8')
    for a, r in eds:
        s = s.replace(a, r)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr0, f
    open(ROOT + f, 'wb').write(out)
    print('%s written, CR %d' % (f, cr0))
