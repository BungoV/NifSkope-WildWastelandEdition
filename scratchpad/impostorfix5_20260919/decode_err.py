"""IMPOSTORFIX5 -- the >12-levels-wrong texel count, on REAL sheets.

IMPOSTORFIX4's row 14c wants "the encoder's input" as the reference. The
encoder's input is the height plane AFTER `lodgenRepairOctHeight` and BEFORE
BC1, and it exists only inside the bake. It is not guessed at here: the C++
function is ported line for line below and the port is CONTROLLED against the
exe's own census line -- whole / partial / outsideNear / outside, four
integers the exe printed for each subject. A port that reproduces all four on
all five subjects is the function; one that does not is refused by name.

Reference  = the ported repair's output blue channel (uint8), from the bake's
             own pre-repair `_oct_normal.png` + `_oct_albedo.png`.
Measured   = blue of the real `_oct_n.DDS` mip 0, decoded.
The number = per cent of sheet texels whose |measured - reference| > 12.
"""
import sys, os, json, glob, re
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'impostorfix1_20260919'))
import bcdec

RAMP = int(os.environ.get('RAMP', '16'))   # 0 = the OLD 8-ring cliff


def repair(nrm_b, alb_a, frameW, frameH, ramp):
    """A line-for-line port of lodgenRepairOctHeight's height half.

    `ramp` = kOutRamp; ramp == 0 selects the OLD code path (kOutRings = 8,
    snap to the card plane beyond it) so the same port measures both exes.
    """
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
                # sum and count of the 8 neighbours that already have a height
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


def census_from_log(path):
    t = open(path, encoding='utf-8', errors='replace').read()
    m = re.search(r'oct height repaired: (\d+) whole kept, (\d+) partial .*?, (\d+) outside '
                  r'(?:within 8 rings|inside the \d+-ring ramp)[^,]*, (\d+) outside set to the card plane', t)
    return tuple(int(x) for x in m.groups()) if m else None


def run(root, tag, ramp):
    d = os.path.join(root, tag, 'cards')
    lodm = glob.glob(os.path.join(d, '*_oct.lodm'))[0]
    raw = open(lodm, 'rb').read()
    j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))
    fw, fh = j['card']['frame']
    nrm = np.asarray(Image.open(glob.glob(os.path.join(d, '*_oct_normal.png'))[0]).convert('RGBA'))
    alb = np.asarray(Image.open(glob.glob(os.path.join(d, '*_oct_albedo.png'))[0]).convert('RGBA'))
    ref, cens = repair(nrm[..., 2], alb[..., 3], fw, fh, ramp)
    exe = census_from_log(os.path.join(root, tag, 'lodgen_repair.log'))
    img, _, _ = bcdec.load_dds(glob.glob(os.path.join(d, '*_oct_n.DDS'))[0])
    got = np.clip(np.rint(img[..., 2] * 255.0), 0, 255).astype(np.int32)
    err = np.abs(got - ref.astype(np.int32))
    pct = 100.0 * float((err > 12).sum()) / err.size
    # block height RANGE of the encoder's input, mean over 4x4 blocks
    h, w = ref.shape
    b = ref[:h // 4 * 4, :w // 4 * 4].reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3)
    brange = float((b.max(axis=(2, 3)).astype(np.int32) - b.min(axis=(2, 3))).mean())
    return dict(tag=tag, pct=pct, mean_err=float(err.mean()), max_err=int(err.max()),
                blk_range=brange, census_port=cens, census_exe=exe,
                census_ok=(exe == cens), texels=int(err.size))


if __name__ == '__main__':
    root = sys.argv[1]
    print('root %s   RAMP=%d' % (root, RAMP))
    print('%-9s %8s %8s %6s %8s  %s' % ('subject', '>12lv%', 'mean', 'max', 'blkrange', 'census port vs exe'))
    for tag in ['blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4']:
        try:
            r = run(root, tag, RAMP)
        except Exception as e:
            print('%-9s  REFUSED: %s' % (tag, e))
            continue
        print('%-9s %8.3f %8.3f %6d %8.3f  %s %s' %
              (r['tag'], r['pct'], r['mean_err'], r['max_err'], r['blk_range'],
               'OK' if r['census_ok'] else 'PORT REFUSED',
               '' if r['census_ok'] else '%s vs %s' % (r['census_port'], r['census_exe'])))
