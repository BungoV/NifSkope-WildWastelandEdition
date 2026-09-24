"""SECTION 1 -- the frame DOWNSAMPLE in the bake (frameOf, src/nifskope_ui.cpp:23102).

The pre-downsample photographs are NOT on disk (report 0). The closest real
input is maple_n4_t256: the SAME subject, SAME half/span/frameOffset, baked at
128x256 texels per frame instead of 32x64. Downsampling it by 4 with each
candidate rule and scoring the result is a measurement of the rule at ratio 4.
The bare trees have no t256 fixture, so they are measured at ratio 2 off their
own shipped frames; absolute numbers there are NOT comparable to the shipped
ones and only the ranking between rules is read.
"""
import sys, os, glob, json, copy
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix2_20260919')
import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt
from inst import *
from bcenc import bc3_roundtrip
from calib import place, mask_at, alpha_ref

CAL = json.load(open('calib.json'))
FLOOR, BASE = 16.0, 160.0

def cov_decode(ae):      # encoded alpha 0..255 -> raw coverage 0..255
    return np.where(ae < BASE, 0.0, FLOOR + (ae - BASE) * (255.0 - FLOOR) / (255.0 - BASE))

def cov_encode(a):       # raw coverage 0..255 -> encoded
    return np.where(a < FLOOR, 0.0, BASE + np.round((a - FLOOR) * (255.0 - BASE) / (255.0 - FLOOR)))

def blocks(x, k):
    H, W = x.shape[:2]
    return x.reshape(H // k, k, W // k, k, *x.shape[2:]).transpose(0, 2, 1, 3, *range(4, 2 + x.ndim))

def downsample_frame(aRaw, ch, k, rule):
    """aRaw: raw coverage 0..255 at source res; ch: (H,W,C) channel values 0..1.
       Returns (aRaw_out, ch_out)."""
    A = blocks(aRaw, k).reshape(aRaw.shape[0] // k, aRaw.shape[1] // k, k * k)
    C = blocks(ch, k).reshape(ch.shape[0] // k, ch.shape[1] // k, k * k, ch.shape[2])
    w = A / 255.0
    ws = w.sum(-1)
    wn = np.where(ws > 0, ws, 1.0)
    box = (C * w[..., None]).sum(2) / wn[..., None]         # coverage-weighted mean
    aBox = A.mean(-1)
    aMax = A.max(-1)
    out = box.copy()
    if rule in ('C_nearest', 'E_max_nearest'):
        big = np.where(w > 0, C[..., 2], np.inf)             # height: nearest = smallest
        h = big.min(2)
        out[..., 2] = np.where(np.isfinite(h), h, 0.5)
    elif rule in ('D_median',):
        m = np.where(w > 0, C[..., 2], np.nan)
        with np.errstate(all='ignore'):
            h = np.nanmedian(m, axis=2)
        out[..., 2] = np.where(np.isnan(h), 0.5, h)
    elif rule in ('F_maxcov_height',):
        i = A.argmax(-1)
        out[..., 2] = np.take_along_axis(C[..., 2], i[..., None], 2)[..., 0]
    aOut = aMax if rule in ('B_maxcov', 'E_max_nearest') else aBox
    return aOut, out

def build(tag, k, rule):
    """Downsample tag's PRE-COMPRESSION sheets by k with `rule`, apply the shipped
       R3 height repair, BC3 round-trip, and return a Sheets-like object."""
    d = '%s/fixture/%s/cards/' % (R1, tag)
    alb = np.asarray(Image.open(glob.glob(d + '*_oct_albedo.png')[0]).convert('RGBA')).astype(float)
    nrm = np.asarray(Image.open(glob.glob(d + '*_oct_normal.png')[0]).convert('RGBA')).astype(float)
    base = Sheets(tag)
    N, fw, fh = base.N, base.fw, base.fh
    oh, ow = alb.shape[0] // k, alb.shape[1] // k
    A2 = np.zeros((oh, ow, 4)); N2 = np.zeros((oh, ow, 4))
    for j in range(N):
        for i in range(N):
            ys, xs = slice(j * fh, (j + 1) * fh), slice(i * fw, (i + 1) * fw)
            aR = cov_decode(alb[ys, xs, 3])
            aC, cC = downsample_frame(aR, alb[ys, xs, :3] / 255.0, k, rule)
            aN, cN = downsample_frame(aR, nrm[ys, xs, :] / 255.0, k, rule)
            oys, oxs = slice(j * fh // k, (j + 1) * fh // k), slice(i * fw // k, (i + 1) * fw // k)
            A2[oys, oxs, :3] = cC; A2[oys, oxs, 3] = cov_encode(aC) / 255.0
            N2[oys, oxs] = cN
            # --- R3, the repair that shipped: dilate height out of FULLY covered
            #     texels, write it on partial ones, card plane outside coverage
            cov = A2[oys, oxs, 3] * 255.0
            h = N2[oys, oxs, 2]
            full = cov >= 250
            if full.any():
                _, idx = distance_transform_edt(~full, return_indices=True)
                h = h[idx[0], idx[1]]
            else:
                h = np.full_like(h, 0.5)
            N2[oys, oxs, 2] = np.where(cov > 0, h, 0.5)
    cs = copy.copy(base)
    cs.alb = bc3_roundtrip(np.clip(A2, 0, 1))
    cs.nrm = bc3_roundtrip(np.clip(N2, 0, 1))
    cs.H, cs.W = cs.alb.shape[:2]; cs.fw, cs.fh = cs.W // N, cs.H // N
    return cs

def score(cs, tag, calibtag=None, crown=0.45):
    c = CAL[calibtag or tag]; s = c['scale']
    tot = []; cr = []
    for v in VIEWS:
        dy, dx = c['off']['%d_%d' % v]
        m = place(mask_at(alpha_ref(cs, dirOf(*v)), cs, s, cs.covFloor), dy, dx)
        g = grabmask(tag, 'after', *v, 'mesh')
        tot.append(iou(m, g))
        ys = np.where(g.any(1))[0]
        if len(ys):
            cut = int(ys.min() + crown * (ys.max() - ys.min()))
            cr.append(iou(m[:cut], g[:cut]))
    return float(np.mean(tot)), float(np.mean(cr))

RULES = ['A_box', 'B_maxcov', 'C_nearest', 'D_median', 'E_max_nearest', 'F_maxcov_height']

if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(); ap.add_argument('job'); a = ap.parse_args()
    if a.job == 'maple':
        print('maple_n4_t256 -> 32x64 (ratio 4), scored on maple_n4\'s own mesh grabs', flush=True)
        base, basec = score(Sheets('maple_n4'), 'maple_n4')
        print('%-18s %8s %8s   (the SHIPPED 32x64 bake: %.4f / crown %.4f)'
              % ('rule', 'IoU', 'crownIoU', base, basec), flush=True)
        t256, t256c = score(Sheets('maple_n4_t256'), 'maple_n4', 'maple_n4')
        print('%-18s %8.4f %8.4f   <- the 128x256 sheet itself, the ceiling' % ('(t256, no cut)', t256, t256c), flush=True)
        for r in RULES:
            cs = build('maple_n4_t256', 4, r)
            m, c = score(cs, 'maple_n4', 'maple_n4')
            print('%-18s %8.4f %8.4f' % (r, m, c), flush=True)
    else:
        for tag in ('blast_n4', 'dead_n4', 'rock_n4'):
            base, basec = score(Sheets(tag), tag)
            print('== %s, its own frames halved (ratio 2). shipped full-res %.4f / crown %.4f'
                  % (tag, base, basec), flush=True)
            for r in RULES:
                cs = build(tag, 2, r)
                m, c = score(cs, tag, tag)
                print('   %-18s %8.4f %8.4f' % (r, m, c), flush=True)
