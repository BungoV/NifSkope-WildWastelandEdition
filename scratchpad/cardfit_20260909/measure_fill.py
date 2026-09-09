#!/usr/bin/env python3
"""Silhouette fill of every frame of every baked card sheet.

Reads the bake's intermediate <id>_oct_albedo.png (coverage in alpha) and the
<id>.txt sidecar's `oct` line for the grid and the frame size, and reports, per
base, the per-axis fill of the silhouette bounding box against the frame and
against the inner rect (frame minus the gutter).
"""
import sys, os, glob, json
import numpy as np
from PIL import Image

COV = 16          # the spec's coverage floor
def gutter(t):    return max(4, t // 16)

def sidecar(p):
    d = {}
    for ln in open(p, encoding='utf-8', errors='replace'):
        t = ln.split()
        if t and t[0] == 'oct':
            d = dict(oct=int(t[1]), fw=int(t[2]), fh=int(t[3]),
                     halfW=float(t[4]), halfH=float(t[5]),
                     cx=float(t[6]), cy=float(t[7]), cz=float(t[8]),
                     depthSpan=float(t[9]), family=t[10], base=int(t[11]))
        if t and t[0] == 'class':
            d['classW'], d['classH'] = int(t[1]), int(t[2])
    return d

def per_base(idpath):
    bid = os.path.basename(idpath)[:-4]
    d = sidecar(idpath)
    if not d: return None
    alb = os.path.join(os.path.dirname(idpath), bid + '_oct_albedo.png')
    if not os.path.exists(alb): return None
    a = np.asarray(Image.open(alb).convert('RGBA'))[:, :, 3]
    N, fw, fh = d['oct'], d['fw'], d['fh']
    G = gutter(max(fw, fh))
    iw, ih = fw - 2 * G, fh - 2 * G
    rows = []
    for j in range(N):
        for i in range(N):
            f = a[j*fh:(j+1)*fh, i*fw:(i+1)*fw]
            m = f >= COV
            if not m.any():
                rows.append((i, j, 0, 0, 0.0, 0.0, None, None)); continue
            ys, xs = np.where(m)
            bw = xs.max() - xs.min() + 1
            bh = ys.max() - ys.min() + 1
            cov = m.sum() / float(fw * fh)
            rows.append((i, j, bw, bh, bw / float(fw), bh / float(fh), bw / float(iw), bh / float(ih)))
    return bid, d, G, iw, ih, rows

def main(dirpath):
    out = []
    for p in sorted(glob.glob(os.path.join(dirpath, '*.txt'))):
        r = per_base(p)
        if r: out.append(r)
    print("%-9s %-5s %-9s %-4s %-7s %-7s   %-21s %-21s" %
          ("base", "oct", "frame", "G", "halfW", "halfH", "fill-x /frame min/med/max", "fill-y /frame min/med/max"))
    agg = {'x': [], 'y': [], 'xi': [], 'yi': []}
    per = []
    for bid, d, G, iw, ih, rows in out:
        fx = np.array([r[4] for r in rows]); fy = np.array([r[5] for r in rows])
        ix = np.array([r[6] for r in rows if r[6] is not None])
        iy = np.array([r[7] for r in rows if r[7] is not None])
        print("%-9s %-5d %dx%-6d %-4d %-7.1f %-7.1f   %.3f/%.3f/%.3f      %.3f/%.3f/%.3f" %
              (bid, d['oct'], d['fw'], d['fh'], G, d['halfW'], d['halfH'],
               fx.min(), np.median(fx), fx.max(), fy.min(), np.median(fy), fy.max()))
        agg['x'].append(fx.max()); agg['y'].append(fy.max())
        agg['xi'].append(ix.max() if len(ix) else 0); agg['yi'].append(iy.max() if len(iy) else 0)
        per.append(dict(base=bid, oct=d['oct'], frame=[d['fw'], d['fh']], G=G,
                        half=[d['halfW'], d['halfH']], inner=[iw, ih],
                        fillx=[float(fx.min()), float(np.median(fx)), float(fx.max())],
                        filly=[float(fy.min()), float(np.median(fy)), float(fy.max())],
                        innerx_max=float(ix.max()) if len(ix) else 0.0,
                        innery_max=float(iy.max()) if len(iy) else 0.0,
                        area_med=float(np.median([r[4]*r[5] for r in rows]))))
    print()
    for k, lbl in (('x', 'BEST-view fill-x /frame'), ('y', 'BEST-view fill-y /frame'),
                   ('xi', 'BEST-view fill-x /inner'), ('yi', 'BEST-view fill-y /inner')):
        v = np.array(agg[k])
        print("%-26s over %2d bases: min %.3f  median %.3f  max %.3f" % (lbl, len(v), v.min(), np.median(v), v.max()))
    json.dump(per, open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fill_before.json'), 'w'), indent=1)

main(sys.argv[1])
