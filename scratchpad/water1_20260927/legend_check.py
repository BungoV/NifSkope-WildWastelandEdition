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
views = ['waterheight', 'watertype', 'bodyid', 'flow', 'shore', 'cellflags']
legs = {}
for v in views + ['default']:
    legs[v] = legend('FL_' + v)


def test(v, px, leg):
    cols = [c for _, c in leg]
    if v in ('watertype', 'cellflags', 'bodyid'):
        return near_any(px, cols)
    if v in ('waterheight', 'shore'):
        return on_segment(px, cols[0], cols[-1])
    if v == 'flow':
        still = [c for l, c in leg if 'still' in l]
        moving = [c for l, c in leg if 'still' not in l]
        k = float(np.median([c.max() for c in moving])) if moving else 255.0
        return on_wheel(px, k) | near_any(px, still)
    raise ValueError(v)


for v in views:
    name, leg = legs[v]
    a = img('FL_' + v).reshape(-1, 3)
    wet = np.abs(a - base).max(axis=1) > 2
    px = a[wet]
    r = {'legend_view': name, 'swatches': len(leg), 'water_px': int(px.shape[0])}
    if px.shape[0] < 500 or not leg:
        r['verdict'] = 'REFUSED (too few water pixels or no legend)'
        out[v] = r
        continue
    ok = test(v, px, leg)
    r['share_matching_legend'] = round(float(ok.mean()), 4)
    r['swatches_present'] = {l: present(px, c) for l, c in leg}
    # floor: another view's legend on these pixels
    other = {'waterheight': 'cellflags', 'watertype': 'cellflags', 'bodyid': 'cellflags', 'flow': 'waterheight',
             'shore': 'cellflags', 'cellflags': 'watertype'}[v]
    try:
        r['floor_with_%s_legend' % other] = round(float(test(v, px, legs[other][1]).mean()), 4)
    except Exception as e:
        r['floor_error'] = str(e)
    out[v] = r

# default: blended plain water
name, leg = legs['default']
a = img('FL_default').reshape(-1, 3)
wet = np.abs(a - base).max(axis=1) > 2
if leg:
    w = leg[0][1]
    pred = 0.60 * w + 0.40 * base[wet]
    ok = np.abs(a[wet] - pred).max(axis=1) <= TOL
    out['default'] = {'legend_view': name, 'legend': leg[0][0] + ' = ' + ','.join(str(int(x)) for x in w),
                      'water_px': int(wet.sum()), 'share_matching_blend_0.60': round(float(ok.mean()), 4)}
    # floor: the blend at a wrong alpha
    pred2 = 0.30 * w + 0.70 * base[wet]
    out['default']['floor_blend_0.30'] = round(float((np.abs(a[wet] - pred2).max(axis=1) <= TOL).mean()), 4)
json.dump(out, open(os.path.join(HERE, 'legend_check.json'), 'w'), indent=1)
for k, r in out.items():
    print(k, {kk: vv for kk, vv in r.items() if kk != 'swatches_present'})
    if 'swatches_present' in r:
        miss = [l for l, n in r['swatches_present'].items() if n == 0]
        print('   swatches missing from the picture:', miss if miss else 'none')
