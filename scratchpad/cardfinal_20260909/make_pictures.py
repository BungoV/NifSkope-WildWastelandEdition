#!/usr/bin/env python3
"""Before / after pictures of two card sheets, at mip 0 and at each library's own
DEEPEST SHIPPED mip.

Four panels a picture:

    BEFORE (lane CARDPAD, mips = 1 + log2(gap))    mip 0 | deepest shipped
    NOW    (lane CARDFINAL, mips = log2(gap),      mip 0 | deepest shipped
            plus per-frame positioning)

Each panel is TWO WHOLE neighbouring frames -- whole, never a band, because a
16-texel-wide frame cropped to a strip reads as stripes rather than as two trees
-- magnified nearest-neighbour, on a checkerboard so transparency is visible,
with the shared frame border drawn in blue and every COVERED texel touching it
marked red. Red is the defect the mip change removes: a bilinear tap taken on
that border picks up HALF the alpha in the neighbour's column, so the number
printed under each panel is `alpha / 2`, the same quantity the lane report and
`measure_perframe.py` use.

WHICH PAIR. The one whose shared border carries the most neighbour alpha in the
BEFORE library at ITS deepest shipped mip -- searched over both orientations,
because a sheet's worst border is as often a row border as a column one (the
first pass searched columns only and photographed 10/255 of a sheet whose worst
is 112/255). If the sheet never bleeds, the tightest pair in the NEW library.
The same pair and orientation are then used in both rows, so the two rows are
the same two views of the same tree.

  make_pictures.py <before_dir> <after_dir> <outdir> <id> [<id> ...]
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw

MAG_TARGET = 460          # a panel is magnified to about this many pixels wide
CELL_W, CELL_H = 470, 470   # every panel occupies the same cell, so no caption clips


def sidecar(p):
    d = {}
    for ln in open(p, encoding='utf-8', errors='replace'):
        t = ln.split()
        if not t:
            continue
        if t[0] == 'oct' and len(t) >= 11:
            d.update(oct=int(t[1]), fw=int(t[2]), fh=int(t[3]))
        elif t[0] == 'gap' and len(t) >= 3:
            d.update(gapX=int(t[1]), gapY=int(t[2]))
        elif t[0] == 'pad' and len(t) >= 3:
            d.update(gapX=2 * int(t[1]), gapY=2 * int(t[2]))
    return d


def boxdown(a):
    h, w = a.shape[:2]
    b = a[:(h // 2) * 2, :(w // 2) * 2].astype(np.uint32)
    acc = b[0::2, 0::2] + b[0::2, 1::2] + b[1::2, 0::2] + b[1::2, 1::2]
    return ((acc + 2) >> 2).astype(np.uint8)


def at_mip(a, k):
    cur = a
    for _ in range(k):
        cur = boxdown(cur)
    return cur


def mips_of(gap, law):
    m, g = (1 if law == '1+log2' else 0), gap
    while g >= 2:
        g //= 2
        m += 1
    return max(1, m)


def pick_worst(a, N, fw, fh, k):
    """(i, j, axis, alpha): the adjacent pair whose shared border carries the most
    neighbour alpha at mip k. axis 'x' = frames (i,j) and (i+1,j) meeting on a
    column; axis 'y' = (i,j) and (i,j+1) meeting on a row."""
    cur = at_mip(a, k)
    fwk, fhk = max(1, fw >> k), max(1, fh >> k)
    best, at = -1, (0, 0, 'x')
    for j in range(N):
        for i in range(N - 1):
            x = (i + 1) * fwk
            col = cur[j * fhk:(j + 1) * fhk, x - 1:x + 1, 3]
            v = int(col.max()) if col.size else 0
            if v > best:
                best, at = v, (i, j, 'x')
    for j in range(N - 1):
        for i in range(N):
            y = (j + 1) * fhk
            row = cur[y - 1:y + 1, i * fwk:(i + 1) * fwk, 3]
            v = int(row.max()) if row.size else 0
            if v > best:
                best, at = v, (i, j, 'y')
    return at, best


def pick_tight(a, N, fw, fh):
    """fallback: the horizontally adjacent pair whose silhouettes come closest"""
    cov = a[:, :, 3] >= 16
    best, at = 10 ** 9, (0, 0, 'x')
    for j in range(N):
        for i in range(N - 1):
            x = (i + 1) * fw
            run = 10 ** 9
            for y in range(j * fh, (j + 1) * fh):
                lo = cov[y, i * fw:x]
                hi = cov[y, x:x + fw]
                if not lo.any() or not hi.any():
                    continue
                run = min(run, int(np.nonzero(hi)[0][0]) + int(fw - 1 - np.nonzero(lo)[0][-1]))
            if run < best:
                best, at = run, (i, j, 'x')
    return at


def panel(a, fw, fh, k, i, j, axis, label):
    """the two whole frames sharing the border, at mip k, magnified, on a board"""
    cur = at_mip(a, k)
    fwk, fhk = max(1, fw >> k), max(1, fh >> k)
    if axis == 'x':
        crop = cur[j * fhk:(j + 1) * fhk, i * fwk:(i + 2) * fwk]
    else:
        crop = cur[j * fhk:(j + 2) * fhk, i * fwk:(i + 1) * fwk]
    h, w = crop.shape[:2]
    mag = max(1, min(MAG_TARGET // max(1, w), 420 // max(1, h)))
    board = Image.new('RGB', (w * mag, h * mag), (210, 210, 214))
    dr = ImageDraw.Draw(board)
    for yy in range(0, h * mag, 8):
        for xx in range(0, w * mag, 8):
            if ((xx // 8) + (yy // 8)) % 2:
                dr.rectangle([xx, yy, xx + 7, yy + 7], fill=(176, 176, 182))
    img = Image.fromarray(crop, 'RGBA').resize((w * mag, h * mag), Image.NEAREST)
    board.paste(img, (0, 0), img)
    dr = ImageDraw.Draw(board)
    if mag >= 4:            # the texel grid, only where a texel is visible at all
        for xx in range(w + 1):
            dr.line([(xx * mag, 0), (xx * mag, h * mag)], fill=(150, 150, 156))
        for yy in range(h + 1):
            dr.line([(0, yy * mag), (w * mag, yy * mag)], fill=(150, 150, 156))
    worst = 0
    if axis == 'x':
        dr.line([(fwk * mag, 0), (fwk * mag, h * mag)], fill=(40, 90, 230), width=max(1, mag // 4))
        cells = [(fwk - 1, yy) for yy in range(h)] + [(fwk, yy) for yy in range(h)]
    else:
        dr.line([(0, fhk * mag), (w * mag, fhk * mag)], fill=(40, 90, 230), width=max(1, mag // 4))
        cells = [(xx, fhk - 1) for xx in range(w)] + [(xx, fhk) for xx in range(w)]
    for (xx, yy) in cells:
        al = int(crop[yy, xx, 3])
        if al > 0:
            worst = max(worst, al)
            dr.rectangle([xx * mag, yy * mag, (xx + 1) * mag - 1, (yy + 1) * mag - 1],
                         outline=(230, 40, 40), width=max(1, mag // 4))
    # A FIXED CELL, content centred. The four panels are wildly different sizes --
    # mip 0 of a 128 frame is 1:1 while the deepest mip is magnified thirteen
    # times -- and a panel sized to its own content clips its own caption.
    out = Image.new('RGB', (max(CELL_W, board.width), max(CELL_H, board.height) + 34), (255, 255, 255))
    out.paste(board, ((out.width - board.width) // 2, 34 + (out.height - 34 - board.height) // 2))
    d2 = ImageDraw.Draw(out)
    d2.text((4, 4), label, fill=(20, 20, 20))
    d2.text((4, 18), 'frame %dx%d   a border tap picks up %d/255 of the neighbour' % (fwk, fhk, worst // 2),
            fill=((190, 30, 30) if worst else (30, 120, 40)))
    return out, worst // 2


def sheet_for(d, ident, law):
    sc = sidecar(os.path.join(d, ident + '.txt'))
    a = np.array(Image.open(os.path.join(d, ident + '_oct_albedo.png')).convert('RGBA'))
    return a, sc, mips_of(min(sc['gapX'], sc['gapY']), law)


def main():
    before, after, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(outdir, exist_ok=True)
    for ident in sys.argv[4:]:
        aB, scB, mB = sheet_for(before, ident, '1+log2')
        aA, scA, mA = sheet_for(after, ident, 'log2')
        N = min(scA['oct'], scB['oct'])
        (i, j, axis), worstB = pick_worst(aB, scB['oct'], scB['fw'], scB['fh'], mB - 1)
        if worstB <= 0:
            i, j, axis = pick_tight(aA, N, scA['fw'], scA['fh'])
        i = min(i, N - (2 if axis == 'x' else 1))
        j = min(j, N - (1 if axis == 'x' else 2))
        cells, worsts = [], []
        for tag, a, sc, M in (('BEFORE  CARDPAD: mips = 1 + log2(gap), one centre for every view', aB, scB, mB),
                              ('NOW  CARDFINAL: mips = log2(gap), per-frame positioning', aA, scA, mA)):
            row = []
            for k in (0, M - 1):
                what = 'mip 0' if k == 0 else 'deepest shipped: mip %d of %d level(s)' % (k, M)
                if M == 1:
                    what = 'mip 0 IS the deepest shipped (1 level)'
                p, w = panel(a, sc['fw'], sc['fh'], k, i, j, axis, '%s   %s' % (tag.split(':')[0], what))
                row.append(p)
                worsts.append(w)
            cells.append(row)
        pw = max(c.width for r in cells for c in r)
        ph = max(c.height for r in cells for c in r)
        out = Image.new('RGB', (2 * pw + 36, 2 * ph + 84), (255, 255, 255))
        dr = ImageDraw.Draw(out)
        dr.text((10, 8), '%s   the two frames sharing the %s border at (%d,%d) of %dx%d   '
                         'blue = that border, red = a covered texel touching it'
                % (ident, 'column' if axis == 'x' else 'row', i, j, N, N), fill=(20, 20, 20))
        dr.text((10, 22), 'BEFORE  %dx%d frame, gap %d,%d, %d mips   [CARDPAD]        '
                          'NOW  %dx%d frame, gap %d,%d, %d mips   [CARDFINAL]'
                % (scB['fw'], scB['fh'], scB['gapX'], scB['gapY'], mB,
                   scA['fw'], scA['fh'], scA['gapX'], scA['gapY'], mA), fill=(20, 20, 20))
        for r in range(2):
            for c in range(2):
                out.paste(cells[r][c], (12 + c * (pw + 12), 44 + r * (ph + 12)))
        p = os.path.join(outdir, 'cardfinal_%s.png' % ident)
        out.save(p)
        print('%s -> %s  (%s border at %d,%d)  border tap picks up: before mip0 %d, before deepest %d;'
              ' now mip0 %d, now deepest %d' % (ident, p, axis, i, j, worsts[0], worsts[1], worsts[2], worsts[3]))


main()
