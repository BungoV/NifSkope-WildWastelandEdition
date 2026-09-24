"""The encoder's input for an octahedral height sheet, and the decode error.

WHAT THIS IS FOR. `impostor_draw.sh` row 14c asks how far the stored `_n`
height decodes from what the bake handed the compressor. That reference
exists only inside `lodgenCard()`: the bake photographs the model into
`*_oct_normal.png`, `lodgenRepairOctHeight` rewrites the blue channel in
memory, and the rewritten image is what BC1 sees. Nothing on disk holds it.

So it is RECONSTRUCTED here -- and the reconstruction is not trusted on its
word. `lodgenRepairOctHeight` prints a census of four integers (whole,
partial, outsideNear, outside) into the bake's own log, and this module
recomputes those four and REFUSES unless all four match. A port that has
drifted from the C++ cannot quietly become the reference: it fails by name.

Both code paths the tree has shipped are here:
  ramp = 0   the 8-ring cliff (exe af457755..ee87eb9e): the object's height
             for eight rings, then a hard step to the card plane
  ramp = 16  the ramp (IMPOSTORFIX5): the object's height faded to the card
             plane over sixteen rings
and the census match picks between them, so the gate does not have to be told
which exe baked the sheets it is looking at.

REFUSALS, all by name and none of them a pass:
  no `*_oct_normal.png` / `*_oct_albedo.png` beside the .lodm  -> REFUSED
  no bake log with a height-repair census line                 -> REFUSED
  neither ramp reproduces the census                           -> REFUSED
"""
import os, re, glob, json
import numpy as np


def _repair(nrm_b, alb_a, frameW, frameH, ramp):
    """A line-for-line port of lodgenRepairOctHeight's height half."""
    H, W = nrm_b.shape
    kFull, kFloor = 250, 16
    out = nrm_b.copy()
    whole = partial = outside = outsideNear = 0
    for fy in range(0, H - frameH + 1, frameH):
        for fx in range(0, W - frameW + 1, frameW):
            a = alb_a[fy:fy + frameH, fx:fx + frameW].astype(np.int32)
            hgt = nrm_b[fy:fy + frameH, fx:fx + frameW].astype(np.int32).copy()
            have = (a >= kFull)
            nFull = int(have.sum())
            if not nFull:
                continue
            whole += nFull
            ring = np.zeros((frameH, frameW), np.int32)
            for p in range(frameW + frameH):
                s = np.zeros((frameH, frameW), np.int64)
                k = np.zeros((frameH, frameW), np.int64)
                hv = np.where(have, hgt, 0)
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        sh = np.zeros_like(hv)
                        kh = np.zeros_like(k)
                        ys0, ys1 = max(0, -dy), frameH - max(0, dy)
                        xs0, xs1 = max(0, -dx), frameW - max(0, dx)
                        yd0, yd1 = max(0, dy), frameH - max(0, -dy)
                        xd0, xd1 = max(0, dx), frameW - max(0, -dx)
                        sh[yd0:yd1, xd0:xd1] = hv[ys0:ys1, xs0:xs1]
                        kh[yd0:yd1, xd0:xd1] = have[ys0:ys1, xs0:xs1]
                        s += sh
                        k += kh
                grow = (~have) & (k > 0)
                if not grow.any():
                    break
                hgt = np.where(grow, (s // np.maximum(k, 1)).astype(np.int32), hgt)
                ring = np.where(grow, p + 1, ring)
                have = have | grow
            sel = (a < kFull)
            part = sel & (a >= kFloor)
            partial += int(part.sum())
            blk = out[fy:fy + frameH, fx:fx + frameW]
            blk = np.where(part, hgt.astype(np.uint8), blk)
            rest = sel & (a < kFloor)
            reach = rest & (ring > 0)
            if ramp:
                r = np.minimum(ring, ramp)
                b = (hgt * (ramp - r) + 128 * r + ramp // 2) // ramp
                blk = np.where(reach, b.astype(np.uint8), blk)
                outsideNear += int((reach & (r < ramp)).sum())
                outside += int((reach & (r >= ramp)).sum())
            else:
                near = reach & (ring <= 8)
                blk = np.where(near, hgt.astype(np.uint8), blk)
                outsideNear += int(near.sum())
                outside += int((reach & ~near).sum())
            far = rest & (ring == 0)
            blk = np.where(far, np.uint8(128), blk)
            outside += int(far.sum())
            out[fy:fy + frameH, fx:fx + frameW] = blk
    return out, (whole, partial, outsideNear, outside)


CENSUS = re.compile(
    r'oct height repaired: (\d+) whole kept, (\d+) partial [^,]*, (\d+) outside '
    r'(?:within 8 rings|inside the \d+-ring ramp)[^,]*, (\d+) outside set to the card plane')


def _census_from_logs(cards_dir):
    """The four integers the bake itself printed, from any log near the sheets."""
    seen = []
    for d in (cards_dir, os.path.dirname(cards_dir.rstrip('/\\'))):
        for p in glob.glob(os.path.join(d, '*.log')):
            try:
                t = open(p, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            for m in CENSUS.finditer(t):
                seen.append(tuple(int(x) for x in m.groups()))
    return seen


def decode_error(lodm_path):
    """Per cent of sheet texels whose decoded height is >12 levels off the
    encoder's input, plus the mean 4x4 block range of that input."""
    from PIL import Image
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from impostor_bc_decode import load_dds

    d = os.path.dirname(os.path.abspath(lodm_path))
    nrm_p = glob.glob(os.path.join(d, '*_oct_normal.png'))
    alb_p = glob.glob(os.path.join(d, '*_oct_albedo.png'))
    dds_p = glob.glob(os.path.join(d, '*_oct_n.DDS')) + glob.glob(os.path.join(d, '*_oct_n.dds'))
    if not nrm_p or not alb_p:
        raise RuntimeError('REFUSED: no *_oct_normal.png / *_oct_albedo.png beside the .lodm -- '
                           'the encoder\'s input cannot be reconstructed from the sheet alone')
    if not dds_p:
        raise RuntimeError('REFUSED: no *_oct_n.DDS beside the .lodm')
    cens = _census_from_logs(d)
    if not cens:
        raise RuntimeError('REFUSED: no bake log with a height-repair census line near the sheets -- '
                           'the port has nothing to be checked against')
    raw = open(lodm_path, 'rb').read()
    j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))
    fw, fh = j['card']['frame']
    nrm = np.asarray(Image.open(nrm_p[0]).convert('RGBA'))
    alb = np.asarray(Image.open(alb_p[0]).convert('RGBA'))
    for ramp in (16, 0):
        ref, got_c = _repair(nrm[..., 2], alb[..., 3], fw, fh, ramp)
        if got_c in cens:
            break
    else:
        raise RuntimeError('REFUSED: neither the ramp nor the 8-ring port reproduces the bake\'s own '
                           'census %s -- the port has drifted from lodgenRepairOctHeight' % (cens,))
    img = load_dds(dds_p[0])
    arr = img[0] if isinstance(img, tuple) else img
    got = np.clip(np.rint(np.asarray(arr)[..., 3 if j.get('nlayout', 1) == 2 else 2] * 255.0), 0, 255).astype(np.int32)
    if got.shape != ref.shape:
        raise RuntimeError('REFUSED: the sheet is %s and the bake\'s PNG is %s' % (got.shape, ref.shape))
    err = np.abs(got - ref.astype(np.int32))
    h, w = ref.shape
    b = ref[:h // 4 * 4, :w // 4 * 4].reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3)
    rng = float((b.max(axis=(2, 3)).astype(np.int32) - b.min(axis=(2, 3))).mean())
    return dict(pct=100.0 * float((err > 12).sum()) / err.size, mean=float(err.mean()),
                worst=int(err.max()), blk_range=rng, ramp=ramp, texels=int(err.size))


if __name__ == '__main__':
    import sys
    r = decode_error(sys.argv[1])
    print('oct height decode: %.3f%% of %d texels more than 12 levels from the encoder\'s input '
          '(mean %.2f, worst %d, mean 4x4 block range %.2f, fill=%s)'
          % (r['pct'], r['texels'], r['mean'], r['worst'], r['blk_range'],
             '%d-ring ramp' % r['ramp'] if r['ramp'] else '8-ring cliff'))
