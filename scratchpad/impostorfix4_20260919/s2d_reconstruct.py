"""DEFECT 2 -- reconstruct the ENCODER'S ACTUAL INPUT, then control on it.

s2c's control failed at 1..17 per cent, and that failure is the finding: the
`_oct_normal.png` beside the DDS is the BAKE's output, written BEFORE
`lodgenDilateFrames` and BEFORE `lodgenRepairOctHeight` (src/lodgen.cpp:3079,
3088 -- the comment at 3102 says the sheets are "converted once from the bake's
PNGs"). So the error s2_chips.py attributed to BC1 was mostly the dilate and
the height repair doing their job, and NONE of those numbers may be quoted as
compression error. They are not quoted.

This rebuilds the encoder's real input by running the two functions the way the
source runs them, in this order:

    lodgenDilateFrames( nrm, alb, tileW, tileH, deep )   img != coverage
    lodgenDilateFrames( alb, alb, tileW, tileH, deep )   img == coverage: alpha kept
    lodgenRepairOctHeight( nrm, alb, tileW, tileH )

with deep = max(8, max(tileW,tileH)/8) (src/lodgen.cpp:3133), and then encodes
mip 0 with the model of lodgenEncodeBC1Block. The control is the SHIPPED bytes.
"""
import os, sys, glob, json
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *
from s2c_encoder import (dds_mip0_blocks, to_blocks, encode_colour, decode_colour,
                         blocks_to_img)

HERE = os.path.dirname(os.path.abspath(__file__))
TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')

def dilate_frame(img, alpha, passes):
    """lodgenDilateFrames on ONE frame, img != coverage (so alpha moves too).
       img: (h,w,4) int. Returns a new array."""
    h, w = alpha.shape
    out = img.copy().astype(np.int64)
    filled = alpha >= 16
    if not filled.any():
        return out.astype(np.uint8)
    mean = (out[filled].sum(0) // filled.sum()).astype(np.int64)
    for _ in range(passes):
        pad = np.zeros((h + 2, w + 2, 4), np.int64); pad[1:-1, 1:-1] = out
        pf = np.zeros((h + 2, w + 2), bool); pf[1:-1, 1:-1] = filled
        s = np.zeros((h, w, 4), np.int64); k = np.zeros((h, w), np.int64)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0: continue
                m = pf[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
                s += pad[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] * m[..., None]
                k += m
        grow = (~filled) & (k > 0)
        if not grow.any():
            break
        val = np.zeros_like(s); nz = k > 0
        val[nz] = s[nz] // k[nz][..., None]
        out[grow] = val[grow]
        filled = filled | grow
    out[~filled] = mean
    return out.astype(np.uint8)

def repair_frame(nrmf, alphaf, kOutRings=8):
    """lodgenRepairOctHeight on ONE frame. Returns (blue, ring)."""
    h, w = alphaf.shape
    hgt = nrmf[..., 2].astype(np.int64).copy()
    have = alphaf >= 250
    ring = np.zeros((h, w), np.int64)
    if not have.any():
        return nrmf[..., 2].copy(), ring
    for p in range(w + h):
        pad = np.zeros((h + 2, w + 2), np.int64); pad[1:-1, 1:-1] = hgt
        pf = np.zeros((h + 2, w + 2), bool); pf[1:-1, 1:-1] = have
        s = np.zeros((h, w), np.int64); k = np.zeros((h, w), np.int64)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dx == 0 and dy == 0: continue
                m = pf[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
                s += pad[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] * m
                k += m
        grow = (~have) & (k > 0)
        if not grow.any():
            break
        hgt[grow] = (s[grow] // k[grow])
        ring[grow] = p + 1
        have = have | grow
    out = nrmf[..., 2].astype(np.int64).copy()
    full = alphaf >= 250
    part = (~full) & (alphaf >= 16)
    near = (~full) & (alphaf < 16) & (ring > 0) & (ring <= kOutRings)
    far = (~full) & (alphaf < 16) & ~near
    out[part] = hgt[part]; out[near] = hgt[near]; out[far] = 128
    return out.astype(np.uint8), ring

def rebuild(cs, kOutRings=8):
    pn = np.asarray(Image.open(glob.glob(cs.dir + '*_oct_normal.png')[0]).convert('RGBA')).astype(np.int64)
    pa = np.asarray(Image.open(glob.glob(cs.dir + '*_oct_albedo.png')[0]).convert('RGBA')).astype(np.int64)
    H, W = pn.shape[:2]
    fw, fh = cs.fw, cs.fh
    deep = max(8, max(fw, fh) // 8)
    nrm = pn.copy().astype(np.uint8)
    ringAll = np.zeros((H, W), np.int64)
    for fy in range(0, H - fh + 1, fh):
        for fx in range(0, W - fw + 1, fw):
            a = pa[fy:fy + fh, fx:fx + fw, 3]
            d = dilate_frame(pn[fy:fy + fh, fx:fx + fw], a, deep)
            b, r = repair_frame(d, a, kOutRings)
            d[..., 2] = b
            nrm[fy:fy + fh, fx:fx + fw] = d
            ringAll[fy:fy + fh, fx:fx + fw] = r
    return nrm, pa[..., 3], ringAll

rows = []
for tag in TAGS:
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    nrm, alpha, ring = rebuild(cs)
    raw, w, h, bw, bh = dds_mip0_blocks(cs.nrmPath)
    sc0 = raw[..., 8].astype(np.uint16) | (raw[..., 9].astype(np.uint16) << 8)
    sc1 = raw[..., 10].astype(np.uint16) | (raw[..., 11].astype(np.uint16) << 8)
    sbits = (raw[..., 12].astype(np.uint32) | (raw[..., 13].astype(np.uint32) << 8)
             | (raw[..., 14].astype(np.uint32) << 16) | (raw[..., 15].astype(np.uint32) << 24))
    B = to_blocks(nrm[..., :3].astype(np.float64))
    c0, c1, bits = encode_colour(B, 'lum')
    okE = float(((c0 == sc0) & (c1 == sc1)).mean()); okB = float((bits == sbits).mean())

    r0, r1, rbits = encode_colour(B, 'pca')
    ref = nrm[:bh * 4, :bw * 4, 2].astype(np.float64)
    dL = blocks_to_img(decode_colour(c0, c1, bits), h, w)
    dP = blocks_to_img(decode_colour(r0, r1, rbits), h, w)
    eL = np.abs(dL[..., 2] - ref); eP = np.abs(dP[..., 2] - ref)
    nL = np.abs(dL[..., :2] - nrm[:bh * 4, :bw * 4, :2]).mean()
    nP = np.abs(dP[..., :2] - nrm[:bh * 4, :bw * 4, :2]).mean()
    rep = cs.nrm.copy(); rep[:bh * 4, :bw * 4, :3] = dP / 255.0
    np.save(os.path.join(HERE, 'nrm_pca_%s.npy' % tag), rep)
    np.save(os.path.join(HERE, 'nrm_true_%s.npy' % tag), nrm)
    rows.append(dict(tag=tag, okE=okE, okB=okB,
                     lum_mean=float(eL.mean()), pca_mean=float(eP.mean()),
                     lum_p95=float(np.percentile(eL, 95)), pca_p95=float(np.percentile(eP, 95)),
                     lum_worst=float(eL.max()), pca_worst=float(eP.max()),
                     nxy_lum=float(nL), nxy_pca=float(nP)))
    print(tag, 'done  endpoints %.2f%%  indices %.2f%%' % (100 * okE, 100 * okB), flush=True)

print()
print('CONTROL: the RECONSTRUCTED input encoded by my model of lodgenEncodeBC1Block,')
print('against the SHIPPED `_n` DDS bytes')
print('%-10s %20s %20s' % ('tag', 'endpoints identical', 'indices identical'))
for r in rows:
    print('%-10s %19.2f%% %19.2f%%' % (r['tag'], 100 * r['okE'], 100 * r['okB']))
print()
print('HEIGHT round-trip on the TRUE input, levels: luminance endpoints vs PCA endpoints')
print('%-10s %8s %8s | %7s %7s | %7s %7s | %8s %8s' %
      ('tag', 'mean L', 'mean P', 'p95 L', 'p95 P', 'wrst L', 'wrst P', 'nXY L', 'nXY P'))
for r in rows:
    print('%-10s %8.2f %8.2f | %7.2f %7.2f | %7.0f %7.0f | %8.2f %8.2f' %
          (r['tag'], r['lum_mean'], r['pca_mean'], r['lum_p95'], r['pca_p95'],
           r['lum_worst'], r['pca_worst'], r['nxy_lum'], r['nxy_pca']))
json.dump(rows, open(os.path.join(HERE, 's2d.json'), 'w'), indent=1)
