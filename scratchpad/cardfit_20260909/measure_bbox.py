#!/usr/bin/env python3
"""Per-frame silhouette bounding boxes of a baked card library.

Reads <id>_oct_albedo.png (coverage in alpha) + <id>.txt (the `oct` line) and
reports, per base, in FRAME TEXELS and in MODEL UNITS:
  - per-view bbox (x0,x1,y0,y1) about the frame centre
  - the UNION of those boxes over all N*N views  = what bungo's rule wants the
    frame to be (one scale, max silhouette bound per axis, centred on it)
  - the drift of the per-view bbox centre, which says whether the frame is
    centred on the object at all
Writes bbox_<tag>.json beside itself.
"""
import sys, os, glob, json, math
import numpy as np
from PIL import Image

COV = 16

def sidecar(p):
    d = {}
    for ln in open(p, encoding='utf-8', errors='replace'):
        t = ln.split()
        if t and t[0] == 'oct' and len(t) >= 12:
            d = dict(oct=int(t[1]), fw=int(t[2]), fh=int(t[3]),
                     halfW=float(t[4]), halfH=float(t[5]),
                     cx=float(t[6]), cy=float(t[7]), cz=float(t[8]),
                     depthSpan=float(t[9]), family=t[10], base=int(t[11]))
        elif t and t[0] == 'class':
            d['classW'], d['classH'] = int(t[1]), int(t[2])
        elif t and t[0] == 'model':
            d['model'] = ' '.join(t[1:])
    return d

def gutter(t):
    return max(4, t // 16)

def per_base(idpath):
    bid = os.path.basename(idpath)[:-4]
    d = sidecar(idpath)
    if not d or 'oct' not in d:
        return None
    alb = os.path.join(os.path.dirname(idpath), bid + '_oct_albedo.png')
    if not os.path.exists(alb):
        return None
    a = np.asarray(Image.open(alb).convert('RGBA'))[:, :, 3]
    N, fw, fh = d['oct'], d['fw'], d['fh']
    G = gutter(max(fw, fh))
    # units per texel: the recorded halves span the FULL frame
    upx = 2.0 * d['halfW'] / fw
    upy = 2.0 * d['halfH'] / fh
    views = []
    for j in range(N):
        for i in range(N):
            f = a[j*fh:(j+1)*fh, i*fw:(i+1)*fw]
            m = f >= COV
            if not m.any():
                views.append(None); continue
            ys, xs = np.where(m)
            x0, x1 = int(xs.min()), int(xs.max()) + 1     # texel half-open
            y0, y1 = int(ys.min()), int(ys.max()) + 1
            views.append((i, j, x0, x1, y0, y1))
    live = [v for v in views if v]
    ux0 = min(v[2] for v in live); ux1 = max(v[3] for v in live)
    uy0 = min(v[4] for v in live); uy1 = max(v[5] for v in live)
    # per-view widths / heights, and centres relative to the frame centre
    ws = np.array([v[3]-v[2] for v in live], float)
    hs = np.array([v[5]-v[4] for v in live], float)
    cxs = np.array([(v[2]+v[3])/2.0 - fw/2.0 for v in live])
    cys = np.array([(v[4]+v[5])/2.0 - fh/2.0 for v in live])
    # what the frame would need to be under bungo's rule, in TEXELS of the
    # present frame, with no padding: the union box
    need_w = ux1 - ux0
    need_h = uy1 - uy0
    return dict(
        base=bid, model=d.get('model',''), oct=N, frame=[fw, fh], G=G,
        half=[d['halfW'], d['halfH']], center=[d['cx'], d['cy'], d['cz']],
        upx=upx, upy=upy,
        union_texels=[ux0, ux1, uy0, uy1],
        union_wh=[need_w, need_h],
        union_units=[need_w*upx, need_h*upy],
        # the union box's centre against the frame centre, in texels
        union_off=[ (ux0+ux1)/2.0 - fw/2.0, (uy0+uy1)/2.0 - fh/2.0 ],
        # symmetric requirement about the CURRENT frame centre
        sym_half_texels=[ max(abs(ux0-fw/2.0), abs(ux1-fw/2.0)),
                          max(abs(uy0-fh/2.0), abs(uy1-fh/2.0)) ],
        view_w=[float(ws.min()), float(np.median(ws)), float(ws.max())],
        view_h=[float(hs.min()), float(np.median(hs)), float(hs.max())],
        cx_drift=[float(cxs.min()), float(np.median(cxs)), float(cxs.max())],
        cy_drift=[float(cys.min()), float(np.median(cys)), float(cys.max())],
        views=len(live), views_total=N*N,
    )

def main(dirpath, tag):
    out = []
    for p in sorted(glob.glob(os.path.join(dirpath, '*.txt'))):
        r = per_base(p)
        if r:
            out.append(r)
    hdr = "%-9s %-9s %-4s %-13s %-13s %-15s %-15s %-13s" % (
        "base", "frame", "G", "union wh(tx)", "union off(tx)", "view w min/med/max",
        "view h min/med/max", "cx drift")
    print(hdr)
    for r in out:
        print("%-9s %dx%-6d %-4d %5.1f,%-7.1f %5.1f,%-7.1f %4.0f/%4.0f/%-5.0f %4.0f/%4.0f/%-5.0f %5.1f..%-5.1f" % (
            r['base'], r['frame'][0], r['frame'][1], r['G'],
            r['union_wh'][0], r['union_wh'][1], r['union_off'][0], r['union_off'][1],
            r['view_w'][0], r['view_w'][1], r['view_w'][2],
            r['view_h'][0], r['view_h'][1], r['view_h'][2],
            r['cx_drift'][0], r['cx_drift'][2]))
    print()
    # aggregate: how much of the frame the UNION box uses, per axis
    ux = np.array([r['union_wh'][0]/r['frame'][0] for r in out])
    uy = np.array([r['union_wh'][1]/r['frame'][1] for r in out])
    print("union-box occupancy of the frame  x: min %.3f med %.3f max %.3f" % (ux.min(), np.median(ux), ux.max()))
    print("union-box occupancy of the frame  y: min %.3f med %.3f max %.3f" % (uy.min(), np.median(uy), uy.max()))
    ox = np.array([abs(r['union_off'][0]) for r in out])
    oy = np.array([abs(r['union_off'][1]) for r in out])
    print("|union centre - frame centre| texels x: min %.1f med %.1f max %.1f" % (ox.min(), np.median(ox), ox.max()))
    print("|union centre - frame centre| texels y: min %.1f med %.1f max %.1f" % (oy.min(), np.median(oy), oy.max()))
    dx = np.array([r['cx_drift'][2]-r['cx_drift'][0] for r in out])
    print("per-view bbox-centre x drift (texels): min %.1f med %.1f max %.1f" % (dx.min(), np.median(dx), dx.max()))
    here = os.path.dirname(os.path.abspath(__file__))
    json.dump(out, open(os.path.join(here, 'bbox_%s.json' % tag), 'w'), indent=1)
    print("-> bbox_%s.json  (%d bases)" % (tag, len(out)))

main(sys.argv[1], sys.argv[2])
