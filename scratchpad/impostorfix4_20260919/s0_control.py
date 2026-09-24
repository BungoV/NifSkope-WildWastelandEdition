"""SECTION 0b -- THE KNOWN-ANSWER CONTROL.

IMPOSTORFIX1 measured, IN THE APPLICATION, that blast_n4's card scored
mean IoU 0.8823 (range 0.8563..0.9107) over the SIXTEEN bake directions with
the blend OFF. That number is this lane's entry ticket: the numpy instrument
must reproduce it from the same sheets and the same mesh grabs before any
number it produces afterwards is allowed to count.

Registration is fitted card-against-card, exactly as IMPOSTORFIX2's calib.py
does it, and never against the mesh -- fitting against the mesh would make the
control measure the fit instead of the card.
"""
import os, sys, re, json, math
import numpy as np
from PIL import Image
from scipy.signal import fftconvolve
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *

BAKEDIR = R1 + '/control/blast_n4_bake'

def bakeviews():
    vs = set()
    for f in os.listdir(BAKEDIR):
        m = re.match(r'v_az(\d{3})_el(\d{2})_(card|mesh)\.png$', f)
        if m:
            vs.add((int(m.group(1)), int(m.group(2))))
    return sorted(vs)

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

def place(m, dy, dx, shape):
    out = np.zeros(shape, bool); h, w = m.shape
    y0, x0 = max(0, dy), max(0, dx); y1, x1 = min(shape[0], dy + h), min(shape[1], dx + w)
    if y1 <= y0 or x1 <= x0: return out
    out[y0:y1, x0:x1] = m[y0 - dy:y1 - dy, x0 - dx:x1 - dx]; return out

def run(which, sub, root):
    cs = Sheets('blast_n4', which, root=root, sub=sub)
    VS = bakeviews()
    A = {v: alpha_ref(cs, dirOf(*v), nframes=1, parallax=False) for v in VS}
    C = {v: grabmask_named(BAKEDIR, *v, 'card') for v in VS}
    M = {v: grabmask_named(BAKEDIR, *v, 'mesh') for v in VS}
    shape = C[VS[0]].shape
    smax = min(shape[1] / (2 * cs.half[0]), shape[0] / (2 * cs.half[1]))
    # coarse then fine scale search, card against card
    best = None; step = (smax - 0.45 * smax) / 18.0
    for s in np.arange(0.45 * smax, smax * 1.0001, step):
        tot = 0.0; n = 0
        for v in VS[::3]:
            m = mask_at(A[v], cs, s, cs.covFloor); g = C[v]
            if m.shape[0] > g.shape[0] or m.shape[1] > g.shape[1]: continue
            inter, _, _ = best_offset(m, g); uni = m.sum() + g.sum() - inter
            tot += inter / uni if uni else 0; n += 1
        if n and (best is None or tot / n > best[1]): best = (float(s), tot / n)
    s0 = best[0]
    for s in np.arange(max(0.02, s0 - 2 * step), min(smax, s0 + 2 * step) + 1e-9, step / 8.0):
        tot = 0.0; n = 0
        for v in VS[::3]:
            m = mask_at(A[v], cs, s, cs.covFloor); g = C[v]
            if m.shape[0] > g.shape[0] or m.shape[1] > g.shape[1]: continue
            inter, _, _ = best_offset(m, g); uni = m.sum() + g.sum() - inter
            tot += inter / uni if uni else 0; n += 1
        if n and tot / n > best[1]: best = (float(s), tot / n)
    s = best[0]
    vs = []
    for v in VS:
        m0 = mask_at(A[v], cs, s, cs.covFloor)
        _, dy, dx = best_offset(m0, C[v])           # registration from the CARD grab
        vs.append(iou(place(m0, dy, dx, shape), M[v]))   # score against the MESH grab
    return cs, s, np.array(vs), VS

if __name__ == '__main__':
    print('KNOWN-ANSWER CONTROL: blast_n4, 16 bake directions, blend OFF')
    print('IMPOSTORFIX1 (in the application): mean 0.8823, range 0.8563..0.9107, 16 views')
    print()
    for which, sub, root, label in (('cards_before', 'fixture', R1, 'fix1 cards_before (shipped)'),
                                    ('cards', 'fixture', R1, 'fix1 cards (fix1 repair)'),
                                    ('cards', 'fixture', R3, 'fix3 cards (8-ring, today)')):
        try:
            cs, s, vs, VS = run(which, sub, root)
        except Exception as e:
            print('%-32s  FAILED: %s' % (label, e)); continue
        print('%-32s scale %.4f  mean %.4f  min %.4f  max %.4f  n=%d'
              % (label, s, vs.mean(), vs.min(), vs.max(), len(vs)))
