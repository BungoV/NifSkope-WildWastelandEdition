"""WATER1 legend gate (skill ww-legend-matches-picture).

Renders are WW_RENDER_FLAT=1 (vertex colours only, no texture, no light), so an OPAQUE water pixel is its
vertex rgb. Water pixels = pixels that differ from FL0_default_nowater (the same frame with WW_LODL_WATER=0).
The legend is the "water legend (<view>):" line of each render's own log, parsed here, never typed.

Checks per view:
  categorical (watertype, cellflags, bodyid): share of water pixels within 3/255 of a legend swatch;
  ramps (waterheight, shore): share of water pixels within 3/255 of the segment between the first and last
      legend stops (the ramp is linear, so its stops span it);
  flow: share of water pixels on the hue wheel at the legend's own brightness (max channel = the legend's,
      min channel 0), or equal to the legend's still colour;
  default: blended -- predicted = 0.60 * legend water + 0.40 * the no-water pixel, within 3/255.
Every legend swatch the picture should hold is looked for (present / missing).
Floor: the same test with ANOTHER view's legend must fail (share far below).
"""
import os, re, sys, json
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
P = os.path.join(HERE, 'pics')
TOL = 3.0


def img(n):
    return np.asarray(Image.open(os.path.join(P, n + '.png')).convert('RGB')).astype(np.float64)


def legend(n):
    txt = open(os.path.join(P, n + '.log'), encoding='utf-8', errors='replace').read()
    m = re.search(r'water legend \(([^)]*)\): (.*)', txt)
    if not m:
        return None, []
    items = []
    for part in m.group(2).split('; '):
        mm = re.match(r'\s*(.*?) = (\d+),(\d+),(\d+)', part)
        if mm:
            items.append((mm.group(1), np.array([float(mm.group(2)), float(mm.group(3)), float(mm.group(4))])))
    return m.group(1), items


def near_any(px, cols):
    if not cols:
        return np.zeros(len(px), bool)
    C = np.stack(cols)
    d = np.abs(px[:, None, :] - C[None, :, :]).max(axis=2)
    return d.min(axis=1) <= TOL


def on_segment(px, a, b):
    ab = b - a
    t = np.clip(((px - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
    q = a + t[:, None] * ab
    return np.abs(px - q).max(axis=1) <= TOL + 1.0   # +1: the ramp is rounded per vertex, the segment is not


def on_wheel(px, k):
    mx = px.max(axis=1)
    mn = px.min(axis=1)
    return (np.abs(mx - k) <= TOL + 1.0) & (mn <= TOL + 1.0)


def present(img_px, col):
    return int((np.abs(img_px - col).max(axis=1) <= TOL).sum())


base = img('FL0_default_nowater').reshape(-1, 3)
out = {}
views = ['waterheight', 'watertype', 'bodyid', 'flow', 'shore', 'cellflags', 'depth']
legs = {}
for v in views + ['default']:
    legs[v] = legend('FL_' + v)

# ---- display curve --------------------------------------------------------------------------------------
# Measured 2026-09-27: the FLAT frame is not raw vertex bytes. The same draw path maps a vertex byte through a
# fixed curve (legend 51,217,242 -> 57,223,243; 255,92,203 -> 253,102,211; white ground 255 -> 253). So:
# CALIBRATE the curve from two views (swatch -> the mode of the picture pixels nearest it), then UNDO it on
# the OTHER views' pixels and run the tests above unchanged. Two folds, so every view is tested by a curve
# it did not help build. The curve is pooled over channels, monotone, linear outside the measured range.
FOLDS = [(['cellflags', 'watertype'], ['waterheight', 'bodyid', 'flow', 'shore', 'depth', 'default']),
         (['bodyid', 'watertype'], ['cellflags']),
         (['bodyid', 'cellflags'], ['watertype'])]
# only CATEGORICAL views calibrate: flow and the ramps are continuous, so their pixels are not swatch colours.
# bodyid alone does not reach below 69, and the curve bends at the low end, so it is paired with a low swatch.


def wet_px(v):
    a = img('FL_' + v).reshape(-1, 3)
    return a[np.abs(a - base).max(axis=1) > 2]


def swatch_pairs(v):
    px = wet_px(v)
    cols = [c for _, c in legs[v][1]]
    if not cols or not len(px):
        return []
    C = np.stack(cols)
    d = np.sqrt(((px[:, None, :] - C[None, :, :]) ** 2).sum(axis=2))
    k = d.argmin(axis=1)
    pairs = []
    for i, c in enumerate(cols):
        sel = px[(k == i) & (d.min(axis=1) < 25.0)]
        if len(sel) < 50:
            continue
        u, n = np.unique(sel, axis=0, return_counts=True)
        pairs += list(zip(c, u[n.argmax()]))
    return pairs


def build_curve(cal_views):
    pairs = []
    for v in cal_views:
        pairs += swatch_pairs(v)
    xs = sorted(set(p[0] for p in pairs))
    ys = [float(np.mean([p[1] for p in pairs if p[0] == x])) for x in xs]
    ys = list(np.maximum.accumulate(ys))   # monotone
    return np.array(xs), np.array(ys)


def undo(px, cur):
    xs, ys = cur
    if len(xs) < 2:
        return px
    lo = (xs[1] - xs[0]) / max(ys[1] - ys[0], 1e-6)
    hi = (xs[-1] - xs[-2]) / max(ys[-1] - ys[-2], 1e-6)
    r = np.interp(px, ys, xs)
    r = np.where(px < ys[0], xs[0] + (px - ys[0]) * lo, r)
    r = np.where(px > ys[-1], xs[-1] + (px - ys[-1]) * hi, r)
    return r


def test(v, px, leg):
    cols = [c for _, c in leg]
    if v in ('watertype', 'cellflags', 'bodyid', 'depth'):
        return near_any(px, cols)
    if v in ('waterheight', 'shore'):
        return on_segment(px, cols[0], cols[-1])
    if v == 'flow':
        still = [c for l, c in leg if 'still' in l]
        moving = [c for l, c in leg if 'still' not in l]
        k = float(np.median([c.max() for c in moving])) if moving else 255.0
        return on_wheel(px, k) | near_any(px, still)
    raise ValueError(v)


OTHER = {'waterheight': 'cellflags', 'watertype': 'cellflags', 'bodyid': 'cellflags', 'flow': 'waterheight',
         'shore': 'cellflags', 'cellflags': 'watertype', 'depth': 'bodyid'}


def check_view(v, cur):
    name, leg = legs[v]
    px = undo(wet_px(v), cur)
    r = {'legend_view': name, 'swatches': len(leg), 'water_px': int(px.shape[0])}
    if px.shape[0] < 500 or not leg:
        r['verdict'] = 'REFUSED (too few water pixels or no legend)'
        return r
    r['share_matching_legend'] = round(float(test(v, px, leg).mean()), 4)
    r['swatches_present'] = {l: present(px, c) for l, c in leg}
    try:   # floor: another view's legend on these pixels
        r['floor_with_%s_legend' % OTHER[v]] = round(float(test(v, px, legs[OTHER[v]][1]).mean()), 4)
    except Exception as e:
        r['floor_error'] = str(e)
    return r


def check_default(cur):
    name, leg = legs['default']
    a = img('FL_default').reshape(-1, 3)
    wet = np.abs(a - base).max(axis=1) > 2
    if not leg or wet.sum() < 500:
        return {'verdict': 'REFUSED'}
    xs, ys = cur
    fw = np.interp(leg[0][1], xs, ys)      # the water's own colour through the measured curve, then the blend
    r = {'legend_view': name, 'legend': leg[0][0] + ' = ' + ','.join(str(int(x)) for x in leg[0][1]),
         'water_px': int(wet.sum())}
    for al, key in ((0.60, 'share_matching_blend_0.60'), (0.30, 'floor_blend_0.30')):
        pred = al * fw + (1 - al) * base[wet]
        r[key] = round(float((np.abs(a[wet] - pred).max(axis=1) <= TOL).mean()), 4)
    return r


# the raw test (no curve) is kept as the first row: it is what the curve is for
ident = (np.array([0.0, 255.0]), np.array([0.0, 255.0]))
out['raw_no_curve'] = {v: check_view(v, ident).get('share_matching_legend') for v in views}
for i, (cal, tst) in enumerate(FOLDS):
    cur = build_curve(cal)
    f = {'calibrated_on': cal, 'curve': [(int(x), round(float(y), 1)) for x, y in zip(*cur)]}
    for v in tst:
        f[v] = check_default(cur) if v == 'default' else check_view(v, cur)
    out['fold%d' % (i + 1)] = f
json.dump(out, open(os.path.join(HERE, 'legend_check.json'), 'w'), indent=1)
print('raw (no curve):', out['raw_no_curve'])
for i in range(len(FOLDS)):
    f = out['fold%d' % (i + 1)]
    print('fold%d calibrated on %s, curve %s' % (i + 1, f['calibrated_on'], f['curve']))
    for v, r in f.items():
        if v in ('calibrated_on', 'curve'):
            continue
        print('  ', v, {kk: vv for kk, vv in r.items() if kk != 'swatches_present'})
        if 'swatches_present' in r:
            miss = [l for l, n in r['swatches_present'].items() if n == 0]
            print('      swatches missing from the picture:', miss if miss else 'none')
