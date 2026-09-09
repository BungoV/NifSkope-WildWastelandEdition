#!/usr/bin/env python3
"""CARDPAD -- the zero-spacing CONTROL for the gap metric.

The old control stripped the margins and re-tiled the inner rects. Under the
ALPHA metric that was enough (any neighbour alpha at all was a failure); under
the GAP metric it is not, because a silhouette that does not reach its own inner
rect still leaves a texel of air at the border -- measured 1.000 on the harness
bake, so the control PASSED the check it exists to fail.

The control now crops each frame to its OWN silhouette box and resizes it into a
cell, so every silhouette touches every edge of its cell and two neighbours are
ZERO texels apart. Real texels, no synthetic sheet, and a floor underneath it:
the number of covered texels actually sitting on a cell border is printed and
must be positive, or the control itself is void."""

P = 'tests/spells/lodgen_octahedral.sh'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

OLD = """IW, IH = TW - 2 * PADX, TH - 2 * PADY
ctl = []
for j in range(N):
    for y in range(IH):
        for i in range(N):
            for x in range(IW):
                ctl.append(apx[(j * TH + PADY + y) * alb.width + i * TW + PADX + x])
cg, cs = min_gap(ctl, N * IW, N * IH, IW, IH, MIPS)
print('  CONTROL, margins stripped: narrowest gap %.3f texels over %d samples (must be under 1)' % (cg, cs))
check('the gap check FAILS on a sheet with no margins at all (the metric can see it)', cs > 0 and cg < 0.999)"""

NEW = """IW, IH = TW - 2 * PADX, TH - 2 * PADY
NEAR = Image.Resampling.NEAREST if hasattr(Image, 'Resampling') else Image.NEAREST
cells = []
for j in range(N):
    row = []
    for i in range(N):
        ks = covered(i, j)
        xs = [k % TW for k in ks]; ys = [k // TW for k in ks]
        box = (min(xs), min(ys), max(xs) + 1, max(ys) + 1)
        row.append(list(tile(alb, i, j).crop(box).resize((IW, IH), NEAR).getdata()))
    cells.append(row)
ctl = []
for j in range(N):
    for y in range(IH):
        for i in range(N):
            ctl.extend(cells[j][i][y * IW:(y + 1) * IW])
edge = sum(1 for j in range(N) for i in range(N) for y in range(IH) for x in range(IW)
           if (x in (0, IW - 1) or y in (0, IH - 1)) and cells[j][i][y * IW + x][3] >= 128)
cg, cs = min_gap(ctl, N * IW, N * IH, IW, IH, MIPS)
print('  CONTROL, every silhouette cropped to its own box and filling its cell: %d covered texels sit on a cell border; narrowest gap %.3f texels over %d samples (must be under 1)'
      % (edge, cg, cs))
check('the gap check FAILS on a sheet with no spacing at all (the metric can see it)',
      edge > 0 and cs > 0 and cg < 0.999)"""

assert s.count(OLD) == 1, 'control block: %d' % s.count(OLD)
s = s.replace(OLD, NEW)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('fix4 ok: %d -> %d bytes' % (len(b), len(nb)))
