"""DEFECT 2 -- the rectangular chips at the trunk, at TEXEL level.

Three candidates the brief names, and the discriminator for each:

  (a) BC3/BC1 endpoint error on the HEIGHT channel across a silhouette edge.
      Height is the BLUE of the `_n` sheet. In BC3 the RGB half is a BC1 block:
      4x4 texels, TWO endpoints, 2-bit indices, and blue shares those endpoints
      with normal X and normal Y. Discriminator: decoded-minus-PNG blue, per
      4x4 block, against how much blue RANGE the block has to carry.
  (b) the encoder (our DDS writer choosing bad endpoints for a range it could
      have carried).  Discriminator: compare the real encoder's error with the
      error of an OPTIMAL 2-endpoint fit to the same block's blue.
  (c) the draw's height step. Discriminator: it cannot make a BLOCK-shaped
      artefact -- the draw has no 4x4 structure at all -- so a block-aligned
      error map convicts the sheet, not the draw.

And the lever the brief asks about: `kOutRings = 8` is a CLIFF. Ring<=8 carries
the object's own height; ring 9 snaps to 128. This measures whether the worst
blocks sit on that cliff.
"""
import os, sys, glob, json
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *

OUT = os.path.dirname(os.path.abspath(__file__))
TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')

def png_of(cs, suffix):
    g = glob.glob(cs.dir + '*_oct_%s.png' % suffix)
    return np.asarray(Image.open(g[0]).convert('RGBA')).astype(float) if g else None

def blocks(a, bs=4):
    """reshape HxW -> (H/bs, W/bs, bs, bs)"""
    H, W = a.shape
    return a[:H // bs * bs, :W // bs * bs].reshape(H // bs, bs, W // bs, bs).transpose(0, 2, 1, 3)

def ringmap(alpha, frameW, frameH, kFull=250, kFloor=16):
    """Reproduce lodgenRepairOctHeight's `ring` exactly: Chebyshev distance from
       the nearest FULLY covered (alpha>=250) texel, per frame, 8-connected."""
    H, W = alpha.shape
    ring = np.full((H, W), 32767, np.int32)
    for fy in range(0, H - frameH + 1, frameH):
        for fx in range(0, W - frameW + 1, frameW):
            a = alpha[fy:fy + frameH, fx:fx + frameW]
            have = a >= kFull
            if not have.any():
                continue
            r = np.where(have, 0, 32767).astype(np.int32)
            cur = have.copy()
            for p in range(frameW + frameH):
                nxt = cur.copy()
                pad = np.zeros((frameH + 2, frameW + 2), bool); pad[1:-1, 1:-1] = cur
                nb = np.zeros_like(cur)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0: continue
                        nb |= pad[1 + dy:1 + dy + frameH, 1 + dx:1 + dx + frameW]
                grow = nb & ~cur
                if not grow.any(): break
                r[grow] = p + 1
                nxt |= grow
                cur = nxt
            ring[fy:fy + frameH, fx:fx + frameW] = r
    return ring

def opt2(vals):
    """Best 2-endpoint, 4-level BC1-style fit to a block's values (exhaustive on
       the block's own min/max, 4 interpolants). Returns the max abs error."""
    lo, hi = vals.min(), vals.max()
    if hi - lo < 1e-9:
        return 0.0
    lv = np.array([lo, lo + (hi - lo) / 3.0, lo + 2 * (hi - lo) / 3.0, hi])
    e = np.abs(vals[..., None] - lv[None, :]).min(-1)
    return float(e.max())

rows = []
detail = {}
for tag in TAGS:
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    pn = png_of(cs, 'normal')
    pa = png_of(cs, 'albedo')
    if pn is None:
        print(tag, 'no pre-encode normal PNG'); continue
    dec_b = cs.nrm[..., 2] * 255.0            # decoded from the shipped DDS
    ref_b = pn[..., 2]                        # the sheet BEFORE the DDS encode
    alpha = (pa[..., 3] if pa is not None else cs.alb[..., 3] * 255.0)
    err = np.abs(dec_b - ref_b)
    ring = ringmap(alpha, cs.fw, cs.fh)

    # ---- is the error BLOCK shaped? per-4x4-block max error vs block blue range
    eb = blocks(err); rb = blocks(ref_b)
    bmax = eb.reshape(eb.shape[0], eb.shape[1], -1).max(-1)
    brange = rb.reshape(rb.shape[0], rb.shape[1], -1).ptp(-1)
    # ---- does the ring-8 CLIFF run through the block?
    rgb_ = blocks(np.where(ring <= 8, 1, 0).astype(float))
    cliff = (rgb_.reshape(rgb_.shape[0], rgb_.shape[1], -1).ptp(-1) > 0) & \
            (blocks((ring < 32767).astype(float)).reshape(rgb_.shape[0], rgb_.shape[1], -1).min(-1) > 0)
    # ---- does the SILHOUETTE edge run through the block?
    cov = blocks((alpha >= 16).astype(float))
    edge = cov.reshape(cov.shape[0], cov.shape[1], -1).ptp(-1) > 0

    # ---- encoder vs an OPTIMAL 2-endpoint fit, on the worst blocks
    idx = np.dstack(np.unravel_index(np.argsort(bmax, axis=None)[::-1][:200], bmax.shape))[0]
    ours, best = [], []
    for by, bx in idx:
        v = rb[by, bx].ravel()
        ours.append(bmax[by, bx]); best.append(opt2(v))
    ours, best = np.array(ours), np.array(best)

    rows.append(dict(tag=tag, fw=cs.fw, fh=cs.fh,
                     mean=float(err.mean()), p95=float(np.percentile(err, 95)), worst=float(err.max()),
                     blocks_over16=int((bmax > 16).sum()), nblocks=int(bmax.size),
                     over16_on_edge=float(edge[bmax > 16].mean()) if (bmax > 16).any() else 0.0,
                     over16_on_cliff=float(cliff[bmax > 16].mean()) if (bmax > 16).any() else 0.0,
                     edge_frac=float(edge.mean()), cliff_frac=float(cliff.mean()),
                     top200_ours=float(ours.mean()), top200_optimal=float(best.mean()),
                     range_over16=float(brange[bmax > 16].mean()) if (bmax > 16).any() else 0.0,
                     range_all=float(brange.mean())))
    detail[tag] = dict(err=err, ring=ring, alpha=alpha, dec=dec_b, ref=ref_b, cs=cs, bmax=bmax)
    print('%-10s done' % tag, flush=True)

print()
print('DEFECT 2 -- BC1 endpoint error on the height channel (`_n` blue)')
print('%-10s %6s %6s %6s | %9s %7s | %8s %8s | %8s %8s' %
      ('tag', 'mean', 'p95', 'worst', 'blk>16lv', 'of all', 'on edge', 'on cliff', 'ours200', 'optimal'))
for r in rows:
    print('%-10s %6.2f %6.2f %6.0f | %5d/%4d %6.1f%% | %7.1f%% %7.1f%% | %8.2f %8.2f' %
          (r['tag'], r['mean'], r['p95'], r['worst'], r['blocks_over16'], r['nblocks'],
           100.0 * r['blocks_over16'] / r['nblocks'], 100 * r['over16_on_edge'],
           100 * r['over16_on_cliff'], r['top200_ours'], r['top200_optimal']))
print()
print('base rates (what a block picked at random would score):')
for r in rows:
    print('  %-10s edge %5.1f%%  cliff %5.1f%%   blue range: all %6.2f, bad blocks %6.2f'
          % (r['tag'], 100 * r['edge_frac'], 100 * r['cliff_frac'], r['range_all'], r['range_over16']))
json.dump(rows, open(os.path.join(OUT, 's2_chips.json'), 'w'), indent=1)
np.save(os.path.join(OUT, 's2_detail_keys.npy'), np.array(list(detail.keys())))
