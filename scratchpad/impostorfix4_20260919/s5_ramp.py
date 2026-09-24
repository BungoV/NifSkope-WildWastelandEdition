"""DEFECT 2 -- the one UNTESTED lever, simulated end to end.

WHAT IS BEING TESTED. `lodgenRepairOctHeight` fills the height outside the
silhouette by dilating from the fully covered texels, and at src/lodgen.cpp
`kOutRings = 8` it STOPS: ring <= 8 keeps the dilated height, ring 9 is 128
(the card plane). That is a CLIFF, and a cliff inside a 4x4 gives that block a
large height range, which section 2 measured as the thing bad blocks have in
common (17.7..32.9 levels against a base of 1.8..4.3). IMPOSTORFIX2 tested ring
COUNTS -- 8, 16, whole frame -- and never tested making the step a RAMP.

THE VARIANTS. Only the outside-coverage fill changes. Seeding, the dilation,
the partial band and everything inside coverage are byte-identical.

  today     ring<=8 dilated, ring>8 -> 128                (the shipped rule)
  ramp8_24  ring<=8 dilated, rings 9..24 linear to 128, beyond 128
  ramp1_16  every outside ring r: lerp(dilated, 128, min(1, r/16))

HOW FAR THE SIMULATION GOES. Each variant is run through the SAME pipeline the
shipped sheet went through -- the same dilate, the same repair, the same model
of lodgenEncodeBC1Block, the same decode -- and then through the same renderer,
the same frozen registration and the same 24 views as every other table here.
The baseline row is re-run through that identical path rather than read from
s1, so a difference cannot be a difference of route. IT IS STILL A SIMULATION:
the build lane makes it real.

THE REFUTER. The height is only read by the parallax step, which is SKIPPED
when |dot(ray, frameFwd)| <= 0.15. If a variant moves the 24-view IoU by less
than the instrument's +0.011 bias, the cliff is not the lever and this row is
withdrawn -- it does not become "a small win".
"""
import os, sys, glob, json
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *
from cal4 import alpha_ref, mask_at, place, load
from s2c_encoder import to_blocks, encode_colour, decode_colour, blocks_to_img
from s2d_reconstruct import dilate_frame

HERE = os.path.dirname(os.path.abspath(__file__))
CAL = load()
TAGS = ('blast_n4', 'maple_n4', 'dead_n4', 'rock_n4')


def repair_variant(nrmf, alphaf, mode):
    """lodgenRepairOctHeight with the OUTSIDE fill swapped. Returns blue."""
    h, w = alphaf.shape
    hgt = nrmf[..., 2].astype(np.int64).copy()
    have = alphaf >= 250
    ring = np.zeros((h, w), np.int64)
    if not have.any():
        return nrmf[..., 2].copy()
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
    out = nrmf[..., 2].astype(np.float64).copy()
    full = alphaf >= 250
    part = (~full) & (alphaf >= 16)
    outside = (~full) & (alphaf < 16) & (ring > 0)
    out[part] = hgt[part]
    d = hgt.astype(np.float64)
    if mode == 'today':
        near = outside & (ring <= 8)
        out[near] = d[near]
        out[outside & (ring > 8)] = 128.0
    elif mode == 'ramp8_24':
        near = outside & (ring <= 8)
        out[near] = d[near]
        band = outside & (ring > 8) & (ring <= 24)
        t = np.clip((ring - 8) / 16.0, 0, 1)
        out[band] = d[band] * (1 - t[band]) + 128.0 * t[band]
        out[outside & (ring > 24)] = 128.0
    elif mode == 'ramp1_16':
        t = np.clip(ring / 16.0, 0, 1)
        out[outside] = d[outside] * (1 - t[outside]) + 128.0 * t[outside]
    far = (~full) & (alphaf < 16) & (ring == 0)
    out[far] = 128.0
    return np.clip(np.rint(out), 0, 255).astype(np.uint8)


def build(cs, mode):
    pn = np.asarray(Image.open(glob.glob(cs.dir + '*_oct_normal.png')[0]).convert('RGBA')).astype(np.int64)
    pa = np.asarray(Image.open(glob.glob(cs.dir + '*_oct_albedo.png')[0]).convert('RGBA')).astype(np.int64)
    H, W = pn.shape[:2]
    fw, fh = cs.fw, cs.fh
    deep = max(8, max(fw, fh) // 8)
    nrm = pn.copy().astype(np.uint8)
    for fy in range(0, H - fh + 1, fh):
        for fx in range(0, W - fw + 1, fw):
            a = pa[fy:fy + fh, fx:fx + fw, 3]
            d = dilate_frame(pn[fy:fy + fh, fx:fx + fw], a, deep)
            d[..., 2] = repair_variant(d, a, mode)
            nrm[fy:fy + fh, fx:fx + fw] = d
    B = to_blocks(nrm[..., :3].astype(np.float64))
    c0, c1, bits = encode_colour(B, 'lum')
    dec = blocks_to_img(decode_colour(c0, c1, bits), H, W)
    sheet = cs.nrm.copy()
    sheet[:dec.shape[0], :dec.shape[1], :3] = dec / 255.0
    # the chip statistic, on the same decode
    ref = nrm[:dec.shape[0], :dec.shape[1], 2].astype(np.float64)
    err = np.abs(dec[..., 2] - ref)
    rb = ref.reshape(ref.shape[0] // 4, 4, ref.shape[1] // 4, 4).transpose(0, 2, 1, 3)
    rng = rb.reshape(rb.shape[0], rb.shape[1], -1).ptp(-1)
    return sheet, dict(over12=float((err > 12).mean()), mean=float(err.mean()),
                       p95=float(np.percentile(err, 95)), worst=float(err.max()),
                       range_mean=float(rng.mean()), range_p99=float(np.percentile(rng, 99)))


if __name__ == '__main__':
    res = {}
    for tag in TAGS:
        c = CAL['%s|fixture|cards' % tag]
        s = c['scale']; offs = c['off']
        cs = Sheets(tag, 'cards', root=R3, sub='fixture')
        M = {v: grabmask(R3, tag, 'cards', *v, 'mesh') for v in VIEWS}
        res[tag] = {}
        for mode in ('today', 'ramp8_24', 'ramp1_16'):
            sheet, st = build(cs, mode)
            np.save(os.path.join(HERE, 'nrm_%s_%s.npy' % (mode, tag)), sheet)
            ious, inks = [], []
            for v in VIEWS:
                dy, dx = offs['%d_%d' % v]
                a = alpha_ref(cs, dirOf(*v), nrmOverride=sheet)
                m = place(mask_at(a, cs, s, cs.covFloor), dy, dx)
                ious.append(iou(m, M[v]))
                inks.append(m.sum() / M[v].sum() if M[v].sum() else np.nan)
            st.update(iou=float(np.mean(ious)), worst_iou=float(np.min(ious)),
                      ink=float(np.nanmean(inks)))
            res[tag][mode] = st
            print('%-10s %-9s IoU %.4f (worst %.4f) ink %.3f  >12lv %.2f%%  blkrange %.2f'
                  % (tag, mode, st['iou'], st['worst_iou'], st['ink'],
                     100 * st['over12'], st['range_mean']), flush=True)
            json.dump(res, open(os.path.join(HERE, 's5.json'), 'w'), indent=1)
    print()
    print('DEFECT 2 -- SIMULATED: the outside-coverage fill as a RAMP, 24 views')
    print('%-10s %-10s %8s %8s %7s %9s %10s' %
          ('tag', 'fill', 'IoU', 'worst', 'ink', '>12lv', 'blk range'))
    for tag in TAGS:
        for mode in ('today', 'ramp8_24', 'ramp1_16'):
            r = res[tag][mode]
            print('%-10s %-10s %8.4f %8.4f %7.3f %8.2f%% %10.2f' %
                  (tag, mode, r['iou'], r['worst_iou'], r['ink'],
                   100 * r['over12'], r['range_mean']))
