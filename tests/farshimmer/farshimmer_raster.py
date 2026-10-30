"""lane FARTOGGLE4: the TAA-off coverage rule of a far object's footprint, exact, vectorised (numpy).

The view's MSAA is the standard 4x pattern (D3D/GL standard sample positions, y down, in pixel units):
    (0.375, 0.125) (0.875, 0.375) (0.125, 0.625) (0.625, 0.875)
A sample point p counts when x0 <= p.x < x1 and y0 <= p.y < y1 (the rasteriser's coverage test for an
axis-aligned footprint; the top-left rule differs only on measure-zero edges). With the TAA off the resolved pixel
is the box average of its samples, so a footprint shows a weight of hits / 4 px^2 in that frame and nothing is
carried to the next frame.
"""
import numpy as np

MS4 = np.array([(0.375, 0.125), (0.875, 0.375), (0.125, 0.625), (0.625, 0.875)])


def hits(x0, x1, y0, y1, pattern=MS4):
    """number of sample points of the tiled pattern inside [x0, x1) x [y0, y1) (arrays)"""
    n = 0
    for ox, oy in pattern:
        cx = np.ceil(x1 - ox) - np.ceil(x0 - ox)
        cy = np.ceil(y1 - oy) - np.ceil(y0 - oy)
        n = n + np.maximum(cx, 0) * np.maximum(cy, 0)
    return n


def hull_area(px, py):
    """silhouette area (px^2) of projected boxes: corners (n, 8) in the order kx*4 + ky*2 + kz. A small far box is
    projected affinely to a part in 10^4, and an affine parallelepiped's outline area is |ex x ey| + |ey x ez| +
    |ez x ex| of its three projected edge vectors."""
    e = lambda k: (px[:, k] - px[:, 0], py[:, k] - py[:, 0])
    (ax, ay), (bx, by), (cx, cy) = e(4), e(2), e(1)
    cr = lambda ux, uy, vx, vy: np.abs(ux * vy - uy * vx)
    return cr(ax, ay, bx, by) + cr(bx, by, cx, cy) + cr(cx, cy, ax, ay)


def footprint(f, idx, smin=0.0, gcap=0.0):
    """the footprint rectangle of rows idx in frame f: the outline's area with the box's aspect, centred on the box,
    each side widened to >= smin px (smin 0 = today's rule); gcap > 0 caps the widened area at gcap x the true one
    (both sides scaled back alike, never under their own size)"""
    bw = np.maximum(f['bw'][idx], 1e-6)
    bh = np.maximum(f['bh'][idx], 1e-6)
    ar = np.minimum(f['area'][idx], bw * bh)
    w = np.sqrt(ar * bw / bh)
    h = np.sqrt(ar * bh / bw)
    w2, h2 = np.maximum(w, smin), np.maximum(h, smin)
    if gcap > 0:
        k = np.sqrt(np.minimum(1.0, gcap * w * h / np.maximum(w2 * h2, 1e-12)))
        w2, h2 = np.maximum(w, w2 * k), np.maximum(h, h2 * k)
    cx, cy = f['cx'][idx], f['cy'][idx]
    return cx - w2 / 2, cx + w2 / 2, cy - h2 / 2, cy + h2 / 2, w, h


def blink_census(frames, idx, mins):
    """per rule (index 0 = today's, then one per smin): per row blinks (0 <-> >0 hits between consecutive frames),
    frames with 0 hits, the weight (hits / 4) and the true area per frame"""
    rules = [(0.0, 0.0)] + [(float(s), 0.0) if not isinstance(s, tuple) else s for s in mins]
    out = dict(rules=rules, blinks=[], offs=[], weight=[], switch=[], w=None, h=None, area=None)
    for s, gc in rules:
        hs, ws_, hh_, ars = [], [], [], []
        for f in frames:
            x0, x1, y0, y1, w, h = footprint(f, idx, s, gc)
            hs.append(hits(x0, x1, y0, y1))
            ws_.append(w); hh_.append(h); ars.append(w * h)
        hs = np.array(hs)               # (frames, rows)
        on = hs > 0
        sw = on[1:] != on[:-1]          # (frames-1, rows)
        out['blinks'].append(sw.sum(0))
        out['offs'].append((~on).sum(0))
        out['switch'].append(sw.sum(1))
        out['weight'].append(hs.mean(0) / 4.0)
        if out['w'] is None:
            out['w'] = np.mean(ws_, 0); out['h'] = np.mean(hh_, 0); out['area'] = np.mean(ars, 0)
    return out


def report(res, nframes):
    w, h, area = res['w'], res['h'], res['area']
    lo, hi = np.minimum(w, h), np.maximum(w, h)
    dots = hi < 1.0
    thin = (lo < 1.0) & ~dots
    big = ~dots & ~thin
    n = len(w)
    L = ['population (visible in every frame): %d rows; dots (both sides < 1 px) %d, thin (one side < 1 px) %d, '
         'resolved %d; area median %.3f px^2' % (n, dots.sum(), thin.sum(), big.sum(), float(np.median(area)) if n else 0)]
    for k, (s, gc) in enumerate(res['rules']):
        b, o, wt, sw = res['blinks'][k], res['offs'][k], res['weight'][k], res['switch'][k]
        name = "today's rule (4x coverage, TAA off)" if k == 0 else 'min footprint %.2f px%s' % (
            s, ' area cap %.2fx' % gc if gc > 0 else '')
        blinkers = b > 0
        gain = wt / np.maximum(area, 1e-9)
        sub = dots | thin
        line = ('%-38s rows that blink %6d (dots %d, thin %d, resolved %d); per frame switching on<->off: mean %.1f '
                'max %d; rows off in >= 1 frame %d' % (name, blinkers.sum(), (blinkers & dots).sum(),
                                                      (blinkers & thin).sum(), (blinkers & big).sum(),
                                                      float(sw.mean()) if len(sw) else 0.0,
                                                      int(sw.max()) if len(sw) else 0, (o > 0).sum()))
        if sub.any():
            line += ('; weight / true area over sub-pixel rows: median %.2f p90 %.2f, dots median %.2f; '
                     'total %.2f' % (float(np.median(gain[sub])), float(np.percentile(gain[sub], 90)),
                                     float(np.median(gain[dots])) if dots.any() else 0.0,
                                     float(wt[sub].sum() / max(area[sub].sum(), 1e-9))))
        L.append(line)
    return L
