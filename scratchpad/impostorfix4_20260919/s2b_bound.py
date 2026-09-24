"""DEFECT 2, the discriminator that separates OUR ENCODER from THE PACKING.

s2_chips.py showed the worst blocks' blue error is 53..101 levels while a
4-level fit to that block's BLUE ALONE would cost 4..10. But a BC1 block cannot
spend its line on blue: R, G and B share ONE line and two endpoints. So "blue
alone" is not a reachable bound and convicts nobody.

This computes the reachable one: the best 3-D line (PCA axis + Lloyd
refinement, endpoints NOT quantised to RGB565, so it is optimistic in our
encoder's favour), and reports the BLUE error that ideal encoder would leave.

  ours >> ideal-3D   ->  OUR ENCODER is the defect (a better encoder fixes it)
  ours ~= ideal-3D   ->  THE PACKING is the defect (height must leave the RGB
                         block; that is owed ruling B, and no encoder helps)

Also here, because they are the same decode:
  * the `_n` and `_d` DDS four-cc, so "BC3" is read and not assumed;
  * the height round trip restricted to COVERED texels, which is the number
    IMPOSTORFIX2 s7 item 5 quotes (6.06..9.90 at p95) -- so the two lanes'
    numbers can be compared instead of talked past;
  * DEFECT 1 STAGE C: does the colour dilate bleed COVERAGE? The source says
    no (lodgenDilateFrames's `put` preserves qAlpha when img IS coverage,
    src/lodgen.cpp:2496). The measurement that would refute it: if the dilate
    bled, the texels outside the silhouette would all carry the frame's mean
    alpha -- one repeated non-zero value. Counted below.
"""
import os, sys, glob, json, struct
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *

TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')
HERE = os.path.dirname(os.path.abspath(__file__))

def fourcc(path):
    with open(path, 'rb') as f:
        h = f.read(128)
    return h[84:88].decode('ascii', 'replace')

def ideal_bc1_blue(rgbblocks, iters=12):
    """rgbblocks: (B,16,3) float 0..255. Best unquantised 2-endpoint, 4-level
       line fit per block; returns max |blue error| per block."""
    V = rgbblocks
    mu = V.mean(1, keepdims=True)
    X = V - mu
    # principal axis per block via power iteration on the 3x3 covariance
    C = np.einsum('bij,bik->bjk', X, X)
    a = np.tile(np.array([1.0, 1.0, 1.0]), (V.shape[0], 1))
    for _ in range(24):
        a = np.einsum('bjk,bk->bj', C, a)
        n = np.linalg.norm(a, axis=1, keepdims=True); n[n == 0] = 1
        a = a / n
    t = np.einsum('bij,bj->bi', X, a)
    e0 = mu[:, 0, :] + a * t.min(1, keepdims=True)
    e1 = mu[:, 0, :] + a * t.max(1, keepdims=True)
    for _ in range(iters):
        lv = np.stack([e0, e0 + (e1 - e0) / 3.0, e0 + 2 * (e1 - e0) / 3.0, e1], 1)  # (B,4,3)
        d = ((V[:, :, None, :] - lv[:, None, :, :]) ** 2).sum(-1)                    # (B,16,4)
        idx = d.argmin(-1)
        w = idx / 3.0
        # least squares on e0,e1 given weights: v = e0 + w (e1-e0)
        s0 = (1 - w); s1 = w
        A00 = (s0 * s0).sum(1); A01 = (s0 * s1).sum(1); A11 = (s1 * s1).sum(1)
        B0 = np.einsum('bi,bic->bc', s0, V); B1 = np.einsum('bi,bic->bc', s1, V)
        det = A00 * A11 - A01 * A01
        ok = np.abs(det) > 1e-9
        ne0 = e0.copy(); ne1 = e1.copy()
        ne0[ok] = ((A11[ok, None] * B0[ok] - A01[ok, None] * B1[ok]) / det[ok, None])
        ne1[ok] = ((A00[ok, None] * B1[ok] - A01[ok, None] * B0[ok]) / det[ok, None])
        e0, e1 = ne0, ne1
    lv = np.stack([e0, e0 + (e1 - e0) / 3.0, e0 + 2 * (e1 - e0) / 3.0, e1], 1)
    d = ((V[:, :, None, :] - lv[:, None, :, :]) ** 2).sum(-1)
    idx = d.argmin(-1)
    pick = np.take_along_axis(lv, idx[:, :, None], 1) if False else \
        lv[np.arange(V.shape[0])[:, None], idx]
    return np.abs(pick[..., 2] - V[..., 2]).max(1)

def blocks3(a, bs=4):
    H, W, C = a.shape
    b = a[:H // bs * bs, :W // bs * bs].reshape(H // bs, bs, W // bs, bs, C).transpose(0, 2, 1, 3, 4)
    return b.reshape(-1, bs * bs, C)

rows = []
for tag in TAGS:
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    gn = glob.glob(cs.dir + '*_oct_normal.png'); ga = glob.glob(cs.dir + '*_oct_albedo.png')
    pn = np.asarray(Image.open(gn[0]).convert('RGBA')).astype(np.float64)
    pa = np.asarray(Image.open(ga[0]).convert('RGBA')).astype(np.float64)
    dec = cs.nrm * 255.0
    err = np.abs(dec[..., 2] - pn[..., 2])
    alpha = pa[..., 3]

    V = blocks3(pn[..., :3])
    ideal = ideal_bc1_blue(V)
    ours = np.abs(blocks3(dec[..., :3])[..., 2] - V[..., 2]).max(1)
    worst = np.argsort(ours)[::-1][:200]

    cov = alpha >= 16
    full = alpha >= 250
    rows.append(dict(
        tag=tag,
        nfmt=fourcc(cs.nrmPath), dfmt=fourcc(cs.albPath),
        ours_top200=float(ours[worst].mean()), ideal_top200=float(ideal[worst].mean()),
        ours_all=float(ours.mean()), ideal_all=float(ideal.mean()),
        p95_covered=float(np.percentile(err[cov], 95)) if cov.any() else 0.0,
        p95_full=float(np.percentile(err[full], 95)) if full.any() else 0.0,
        p95_all=float(np.percentile(err, 95)),
        # DEFECT 1 STAGE C: the coverage-bleed refuter
        out_nonzero=int((alpha[~cov] > 0).sum()), out_total=int((~cov).sum()),
        out_uniq=int(len(np.unique(alpha[~cov]))),
        dds_out_nonzero=int((cs.alb[..., 3][~cov] * 255 > 0.5).sum()),
    ))
    print(tag, 'done', flush=True)

print()
print('DEFECT 2 -- is it OUR ENCODER or THE PACKING? (height = `_n` blue, in the BC1 half)')
print('%-10s %6s %6s | %10s %10s %7s | %10s %10s' %
      ('tag', '_n', '_d', 'ours@200', 'ideal3D@200', 'ratio', 'ours all', 'ideal all'))
for r in rows:
    print('%-10s %6s %6s | %10.2f %10.2f %7.2fx | %10.3f %10.3f' %
          (r['tag'], r['nfmt'], r['dfmt'], r['ours_top200'], r['ideal_top200'],
           r['ours_top200'] / max(r['ideal_top200'], 1e-9), r['ours_all'], r['ideal_all']))
print()
print('height round trip, levels (IMPOSTORFIX2 s7 quotes p95 6.06..9.90 on COVERED texels)')
print('%-10s %12s %12s %12s' % ('tag', 'p95 covered', 'p95 full', 'p95 all texels'))
for r in rows:
    print('%-10s %12.2f %12.2f %12.2f' % (r['tag'], r['p95_covered'], r['p95_full'], r['p95_all']))
print()
print('DEFECT 1 STAGE C -- does the colour dilate bleed COVERAGE?')
print('%-10s %22s %22s %10s' % ('tag', 'PNG alpha>0 outside cov', 'DDS alpha>0 outside cov', 'uniq vals'))
for r in rows:
    print('%-10s %12d / %-8d %12d / %-8d %10d' %
          (r['tag'], r['out_nonzero'], r['out_total'], r['dds_out_nonzero'], r['out_total'], r['out_uniq']))
json.dump(rows, open(os.path.join(HERE, 's2b_bound.json'), 'w'), indent=1)
