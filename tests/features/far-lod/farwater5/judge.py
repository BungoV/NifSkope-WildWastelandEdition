"""judge.py export|judge <dump dir> <work dir>: FARWATER5 L3 over lodgen's river dumps (WW_WATER_FIELD_DUMP of a bake:
river_*.wwfd; 'agree' = the bake's median = code 8). export writes ratios.f64 (speed / median of every solved texel) +
moving.u8 (>= 0.02 x the piece mean, the writer's slack line); judge reads codes.out from flowcode_unit and checks:
  B1 the linear codes == the old writer's floor(8 r + 0.5) clipped 0..15 on every texel (the off path's bytes)
  B2 the log codes == round(8 + 2 log2 r) clipped (FARWATER4 nibl.py's rule) on every texel
  B3 over-range (the nibble cannot carry the speed: x > 15.5) < 5 % of solved texels under the log code;
     the red (the linear code) must FAIL it
  B4 the log decode of moving water inside the code's range within a quarter octave of the true speed
Prints one line a bar and FARWATER5 L3 RESULT PASS|FAIL."""
import os, struct, sys
import numpy as np


def load(p):	# notes/fix1030c/farwater3/tools/wwfd.py, copied (the tree's tests do not read notes)
    b = open(p, 'rb').read()
    assert b[:4] == b'WWFD', p
    ver, bid, W, H, px0, py0, used, _ = struct.unpack_from('<8i', b, 4)
    ms, agree = struct.unpack_from('<2d', b, 36)
    o = 52
    n = W * H
    out = {}
    for k, dt in (('mask', '<u1'), ('rhs', '<f8'), ('ux', '<f8'), ('uy', '<f8'), ('vx', '<f4'), ('vy', '<f4'),
                  ('wgt', '<f4')):
        a = np.frombuffer(b, dtype=dt, count=n, offset=o)
        o += a.nbytes
        out[k] = a.reshape(H, W)
    assert o == len(b), (o, len(b))
    out['mask'] = out['mask'].astype(bool)
    out['meanSpeed'], out['agree'] = ms, agree
    return out


def export(dd, wd):
    rs, mv = [], []
    n = 0
    for f in sorted(os.listdir(dd)):
        if not f.endswith('.wwfd'):
            continue
        d = load(os.path.join(dd, f))
        m = d['mask']
        sp = np.hypot(d['ux'], d['uy'])
        rs.append((sp / d['agree'])[m])
        mv.append((sp >= 0.02 * d['meanSpeed'])[m])
        n += 1
    r = np.concatenate(rs).astype('<f8')
    r.tofile(os.path.join(wd, 'ratios.f64'))
    np.concatenate(mv).astype('u1').tofile(os.path.join(wd, 'moving.u8'))
    print('exported %d river dumps, %d solved texels' % (n, r.size))


def judge(wd):
    r = np.fromfile(os.path.join(wd, 'ratios.f64'), '<f8')
    mv = np.fromfile(os.path.join(wd, 'moving.u8'), 'u1').astype(bool)
    c = np.fromfile(os.path.join(wd, 'codes.out'), 'u1').reshape(-1, 2)
    lin, lg = c[:, 0].astype(int), c[:, 1].astype(int)
    fails = 0
    eL = np.clip(np.floor(8 * r + 0.5), 0, 15).astype(int)
    with np.errstate(divide='ignore'):
        xg = 8 + 2 * np.log2(np.maximum(r, 1e-300))
    eG = np.where(r > 0, np.clip(np.floor(xg + 0.5), 0, 15), 0).astype(int)
    d1, d2 = int((lin != eL).sum()), int((lg != eG).sum())
    print('B1 linear codes vs the old writer: %d of %d differ -> %s' % (d1, r.size, 'ok' if d1 == 0 else 'FAIL'))
    print('B2 log codes vs nibl.py log rule: %d of %d differ -> %s' % (d2, r.size, 'ok' if d2 == 0 else 'FAIL'))
    fails += (d1 != 0) + (d2 != 0)
    oL, oG = int((8 * r > 15.5).sum()), int((xg > 15.5).sum())
    sL, sG = int((lin == 15).sum()), int((lg == 15).sum())
    bar = 0.05 * r.size
    print('B3 over-range (x > 15.5): log %d (%.2f %%), code 15 on %d -> %s' % (
        oG, 100.0 * oG / r.size, sG, 'ok' if oG < bar else 'FAIL'))
    print('   red linear: over-range %d (%.2f %%), code 15 on %d -> %s' % (
        oL, 100.0 * oL / r.size, sL, 'FAILS the bar (red ok)' if oL >= bar else 'PASSES (red not red: FAIL)'))
    fails += (oG >= bar) + (oL < bar)
    inr = mv & (xg >= 0.5) & (xg <= 15.5)
    q = np.exp2((lg[inr] - 8) * 0.5) / r[inr]
    e = np.maximum(q, 1 / q) - 1
    lim = 2 ** 0.25 - 1 + 1e-9
    qL = (lin[inr] / 8.0) / r[inr]
    eLn = np.where(qL > 0, np.maximum(qL, 1 / np.maximum(qL, 1e-300)) - 1, np.inf)
    print('B4 moving texels in range %d: log decode worst error %.4f p99 %.4f (bar %.4f) -> %s; red linear worst %.4g p99 %.4g'
          % (inr.sum(), e.max(), np.percentile(e, 99), lim, 'ok' if e.max() <= lim else 'FAIL', eLn.max(),
             np.percentile(eLn, 99)))
    fails += e.max() > lim
    print('FARWATER5 L3 RESULT %s' % ('PASS' if fails == 0 else 'FAIL'))
    return 1 if fails else 0


if __name__ == '__main__':
    if sys.argv[1] == 'export':
        export(sys.argv[2], sys.argv[3])
    else:
        sys.exit(judge(sys.argv[3]))
