"""DEFECT 4 -- IoU against ON-SCREEN HEIGHT, for N=4 and N=8, with and without
a mip.

WHY THIS IS THE RIGHT EXPERIMENT. Every sheet fetch in impostor_oct.frag is
`textureLod( ..., 0.0 )` (lines 217, 267, 275, 276, 295, 297). `cardMipCap` is
uploaded at src/gl/impostordraw.cpp:466, declared at impostor_oct.frag:72, and
never read. So at every apparent size the card takes ONE bilinear tap out of
mip 0. When the card is 16 px tall and the frame is 128 texels wide, one pixel
spans eight texels and seven of them are never looked at. That is the
mechanism this measures.

HOW. For an on-screen height Hs the card is RENDERED AT Hs PIXELS -- not
downsampled from a big render, because the aliasing is the whole point. The
mesh reference at the same size is the 24-view mesh grab AREA-AVERAGED down by
the same factor and cut at 0.5 coverage: what a pixel would hold if it were
covered more than half. The frozen registration from cal4 is scaled by the
same factor, so nothing is re-fitted per size.

  mip 0       what the shader does today
  mip = k     both sheets box-downsampled k times before sampling, k chosen so
              one screen pixel is about one texel: k = round(log2(frameW/Wcard))
              clamped to >= 0. This is a SIMULATION of a mip chain, not a mip
              chain: a real GPU trilinearly blends two levels. It is therefore
              a LOWER bound on what correct mip selection buys, not an upper.

THE REFUTER for "mip selection is the lever": if the mip rows do NOT beat mip 0
below the knee, the thinness is the card's geometry or the coverage floor and
not the sampling, and row 4 of section 5 must be withdrawn.
"""
import os, sys, json
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *
from cal4 import load, SHAPE

HERE = os.path.dirname(os.path.abspath(__file__))
CAL = load()
TAGS = ('blast_n4', 'blast_n8', 'maple_n4')
SIZES = (8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256)


def area_down(mask, f):
    """Area-average a boolean mask by factor f (<1 shrinks), cut at 0.5."""
    h, w = mask.shape
    H, W = max(1, int(round(h * f))), max(1, int(round(w * f)))
    ys = (np.arange(H + 1) * h / H).round().astype(int)
    xs = (np.arange(W + 1) * w / W).round().astype(int)
    c = np.cumsum(np.cumsum(mask.astype(np.float64), 0), 1)
    c = np.pad(c, ((1, 0), (1, 0)))
    S = c[ys[1:, None], xs[None, 1:]] - c[ys[:-1, None], xs[None, 1:]] \
        - c[ys[1:, None], xs[None, :-1]] + c[ys[:-1, None], xs[None, :-1]]
    A = (ys[1:, None] - ys[:-1, None]) * (xs[None, 1:] - xs[None, :-1])
    return (S / np.maximum(A, 1)) >= 0.5


def place_small(m, dy, dx, shape):
    out = np.zeros(shape, bool)
    h, w = m.shape
    y0, x0 = max(0, dy), max(0, dx)
    y1, x1 = min(shape[0], dy + h), min(shape[1], dx + w)
    if y1 <= y0 or x1 <= x0:
        return out
    out[y0:y1, x0:x1] = m[y0 - dy:y1 - dy, x0 - dx:x1 - dx]
    return out


def run(tag):
    c = CAL['%s|fixture|cards' % tag]
    s = c['scale']
    offs = c['off']
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    cardH_big = 2 * cs.half[1] * s                 # card height on the 768x512 canvas
    M = {v: grabmask(R3, tag, 'cards', *v, 'mesh') for v in VIEWS}
    rows = []
    for Hs in SIZES:
        f = Hs / cardH_big
        shape = (max(4, int(round(SHAPE[0] * f))), max(4, int(round(SHAPE[1] * f))))
        Ws = max(2, int(round(2 * cs.half[0] * s * f)))
        kbest = max(0, int(round(np.log2(max(1.0, cs.fw / max(Ws, 1))))))
        ref = {v: area_down(M[v], f) for v in VIEWS}
        rec = dict(px=Hs, wpx=Ws, mip=kbest)
        for name, k in (('mip0', 0), ('mipk', kbest)):
            ious, inks = [], []
            for v in VIEWS:
                dy, dx = offs['%d_%d' % v]
                _, a = render(cs, dirOf(*v), (Ws, Hs), mip=k)
                m = place_small(a >= cs.covFloor,
                                int(round(dy * f)), int(round(dx * f)), shape)
                r = ref[v]
                ious.append(iou(m, r))
                inks.append(m.sum() / r.sum() if r.sum() else np.nan)
            rec[name] = dict(iou=float(np.mean(ious)), worst=float(np.min(ious)),
                             ink=float(np.nanmean(inks)))
        rows.append(rec)
        print('  %-10s %4d px  mip0 %.4f  mip%d %.4f' %
              (tag, Hs, rec['mip0']['iou'], kbest, rec['mipk']['iou']), flush=True)
    return rows


if __name__ == '__main__':
    res = {}
    for tag in TAGS:
        res[tag] = run(tag)
        json.dump(res, open(os.path.join(HERE, 's4.json'), 'w'), indent=1)
    print()
    print('DEFECT 4 -- IoU vs ON-SCREEN HEIGHT, 24 views, frozen registration')
    print('%-10s %5s %5s %4s | %8s %8s %6s | %8s %8s %6s' %
          ('tag', 'px', 'wpx', 'mip', 'IoU@mip0', 'worst', 'ink', 'IoU@mipk', 'worst', 'ink'))
    for tag in TAGS:
        for r in res[tag]:
            print('%-10s %5d %5d %4d | %8.4f %8.4f %6.3f | %8.4f %8.4f %6.3f' %
                  (tag, r['px'], r['wpx'], r['mip'],
                   r['mip0']['iou'], r['mip0']['worst'], r['mip0']['ink'],
                   r['mipk']['iou'], r['mipk']['worst'], r['mipk']['ink']))
