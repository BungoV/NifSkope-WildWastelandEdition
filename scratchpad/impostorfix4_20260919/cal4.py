"""Calibrate the instrument on the sheets IMPOSTORFIX3 actually shipped
(scratchpad/impostorfix3_20260919/fixture/<tag>/cards) over the SAME 24 orbit
views, and check it reproduces the harness's own printed IoU.

Registration (one scale, one integer offset per view) is fitted CARD-against-
CARD, never against the mesh. Cached in calib4.json so the ablations below do
not refit anything -- every stage table in this report is scored through ONE
frozen registration, which is what makes the rows comparable.
"""
import os, sys, json
import numpy as np
from PIL import Image
from scipy.signal import fftconvolve
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *

HERE = os.path.dirname(os.path.abspath(__file__))
CAL = os.path.join(HERE, 'calib4.json')

# IMPOSTORFIX3's own in-application 24-view means (its report, section 1 table)
HARNESS8 = {'blast_n4': 0.5736, 'blast_n8': 0.7483, 'maple_n4': 0.3674,
            'dead_n4': 0.6073, 'rock_n4': 0.8305}
# and the R3 control it re-measured on the same exe
HARNESSR3 = {'blast_n4': 0.5038, 'blast_n8': 0.6754, 'maple_n4': 0.3545,
             'dead_n4': 0.5721, 'rock_n4': 0.7724}
TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')
SHAPE = (768, 512)

def best_offset(card, target):
    a = target.astype(np.float32); b = card.astype(np.float32)[::-1, ::-1]
    c = fftconvolve(a, b, mode='full')
    idx = np.unravel_index(np.argmax(c), c.shape); bh, bw = card.shape
    return float(c[idx]), idx[0] - (bh - 1), idx[1] - (bw - 1)

def alpha_ref(cs, d, **kw):
    H = 1024; W = max(4, int(round(H * cs.half[0] / cs.half[1])))
    _, a = render(cs, d, (W, H), **kw)
    return a

def mask_at(a, cs, s, thr):
    W = max(4, int(round(2 * cs.half[0] * s))); H = max(4, int(round(2 * cs.half[1] * s)))
    z = np.asarray(Image.fromarray(a.astype(np.float32)).resize((W, H), Image.BILINEAR))
    return z >= thr

def place(m, dy, dx, shape=SHAPE):
    out = np.zeros(shape, bool); h, w = m.shape
    y0, x0 = max(0, dy), max(0, dx); y1, x1 = min(shape[0], dy + h), min(shape[1], dx + w)
    if y1 <= y0 or x1 <= x0: return out
    out[y0:y1, x0:x1] = m[y0 - dy:y1 - dy, x0 - dx:x1 - dx]; return out

def meshes(tag, which='cards'):
    return {v: grabmask(R3, tag, which, *v, 'mesh') for v in VIEWS}

def calibrate(tag, which='cards', sub='fixture'):
    cs = Sheets(tag, which, root=R3, sub=sub)
    A = {v: alpha_ref(cs, dirOf(*v)) for v in VIEWS}
    G = {v: grabmask(R3, tag, which, *v, 'card') for v in VIEWS}
    sub12 = VIEWS[0:12:3] + VIEWS[12:24:3]
    smax = min(SHAPE[1] / (2 * cs.half[0]), SHAPE[0] / (2 * cs.half[1]))
    best = None; step = (smax - 0.45 * smax) / 18.0
    for s in np.arange(0.45 * smax, smax * 1.0001, step):
        tot = 0.0; n = 0
        for v in sub12:
            m = mask_at(A[v], cs, s, cs.covFloor); g = G[v]
            if m.shape[0] > g.shape[0] or m.shape[1] > g.shape[1]: continue
            inter, _, _ = best_offset(m, g); uni = m.sum() + g.sum() - inter
            tot += inter / uni if uni else 0; n += 1
        if n and (best is None or tot / n > best[1]): best = (float(s), tot / n)
    s0 = best[0]
    for s in np.arange(max(0.02, s0 - 2 * step), min(smax, s0 + 2 * step) + 1e-9, step / 8.0):
        tot = 0.0; n = 0
        for v in sub12:
            m = mask_at(A[v], cs, s, cs.covFloor); g = G[v]
            if m.shape[0] > g.shape[0] or m.shape[1] > g.shape[1]: continue
            inter, _, _ = best_offset(m, g); uni = m.sum() + g.sum() - inter
            tot += inter / uni if uni else 0; n += 1
        if n and tot / n > best[1]: best = (float(s), tot / n)
    s = best[0]; offs = {}; cc = []
    for v in VIEWS:
        m = mask_at(A[v], cs, s, cs.covFloor); g = G[v]
        inter, dy, dx = best_offset(m, g); uni = m.sum() + g.sum() - inter
        offs['%d_%d' % v] = [int(dy), int(dx)]; cc.append(inter / uni)
    return cs, s, offs, float(np.mean(cc))

def load():
    return json.load(open(CAL)) if os.path.exists(CAL) else {}

if __name__ == '__main__':
    out = load()
    print('%-10s %-13s %6s %10s %12s %10s %8s' % ('tag', 'set', 'scale', 'card-card', 'npy-vs-mesh', 'harness', 'delta'), flush=True)
    for which, sub, HH, label in (('cards', 'fixture', HARNESS8, '8-ring'),
                                  ('cards', 'fixture_r3', HARNESSR3, 'R3')):
        for tag in TAGS:
            key = '%s|%s|%s' % (tag, sub, which)
            cs, s, offs, cc = calibrate(tag, which, sub)
            M = {v: grabmask(R3, tag, 'cards' if sub == 'fixture' else 'cards_r3', *v, 'mesh') for v in VIEWS}
            vs = []
            for v in VIEWS:
                dy, dx = offs['%d_%d' % v]
                m = place(mask_at(alpha_ref(cs, dirOf(*v)), cs, s, cs.covFloor), dy, dx)
                vs.append(iou(m, M[v]))
            mm = float(np.mean(vs))
            out[key] = {'scale': s, 'off': offs, 'cardcard': cc, 'npy_mesh': mm,
                        'harness': HH[tag], 'sub': sub, 'which': which, 'tag': tag}
            print('%-10s %-13s %6.3f %10.4f %12.4f %10.4f %+8.4f'
                  % (tag, label, s, cc, mm, HH[tag], mm - HH[tag]), flush=True)
            json.dump(out, open(CAL, 'w'), indent=1)
    print('wrote', CAL, flush=True)
