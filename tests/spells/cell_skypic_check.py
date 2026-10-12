"""THE PROBES' SKY FROM THE DRAWN SKY, REBUILT HERE (lane SKYPIC1, 2026-10-11; tests/spells/cell_skypic.sh).

This file shares no code with the viewer. It reads what the viewer dumped beside the bake (WW_CELL_GI_DUMP):
  gi_skypic.bin   "SKYPIC1\\0", int32 n, then 6 faces x n x n x rgb float32: the sky the view drew, as light
  gi_skypic.txt   the capture's key, its faces' axes, and the viewer's table q <axis> <octant> r g b
  gi_sky.bin      int32 count, then per probe 6 axes x rgb float32: the sky the viewer gave each probe
and the bake's .tbk files (its own reader, tests/spells/probe_bake.read_tbk): per probe 8 sky visibilities and 8
glass tints. Stages:
  F  the faces: the viewer's face axes are the cube this file assumes (+X -X +Y -Y +Z -Z forward; up +Z for the
     four sides, +Y for the poles; right = forward x up)
  Q  the table: per axis and octant, the integral of the drawn light x max(axis . dir, 0) over that octant,
     integrated HERE (each texel's exact solid angle, the cosine at its centre), against the viewer's q lines
  S  the probes: per probe and axis, sum over octants of visibility x tint x Q (this file's Q), against gi_sky.bin
Modes:
  check <run> <dump>                     F Q S on <run>/<dump>, <run>/bake/*.tbk;  last line "skypic PASS|FAIL"
  flat <run> <dumpA> <dumpB>             K: the two dumps' gi_sky.bin agree (the known-answer control: a sky of one
                                         colour drawn and integrated = SKY1's flat sky of that colour)
  zenith <shot.png> <run> <dump>         D: the shot (tilted to the zenith, exposure 0 EV, view Standard) at its
                                         centre against the capture's +Z face at its centre, sRGB-encoded here; and
                                         the -Z face's centre against the same shot, which must NOT match
"""
import math
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import read_tbk  # noqa: E402

FWD = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)
UP = np.array([[0, 0, 1], [0, 0, 1], [0, 0, 1], [0, 0, 1], [0, 1, 0], [0, 1, 0]], float)
AXES = FWD


def read_pic(d):
    b = open(os.path.join(d, 'gi_skypic.bin'), 'rb').read()
    if b[:8] != b'SKYPIC1\0':
        raise ValueError('gi_skypic.bin: bad magic')
    n = struct.unpack_from('<i', b, 8)[0]
    px = np.frombuffer(b, '<f4', 6 * n * n * 3, 12).reshape(6, n, n, 3).astype(np.float64)
    return n, px


def read_txt(d):
    faces, q = {}, np.zeros((6, 8, 3))
    seen = 0
    for ln in open(os.path.join(d, 'gi_skypic.txt'), encoding='utf-8'):
        w = ln.split()
        if not w:
            continue
        if w[0] == 'face':
            v = [float(x) for x in (w[3:6] + w[7:10] + w[11:14])]
            faces[int(w[1])] = np.array(v).reshape(3, 3)
        elif w[0] == 'q':
            q[int(w[1]), int(w[2])] = [float(x) for x in w[3:6]]
            seen += 1
    return faces, q, seen


def own_q(n, px):
    """per axis 6 x octant 8 x rgb"""
    e = np.linspace(-1.0, 1.0, n + 1)
    c = 0.5 * (e[:-1] + e[1:])

    def A(x, y):
        return np.arctan2(x * y, np.sqrt(x * x + y * y + 1.0))
    X0, Y0 = np.meshgrid(e[:-1], e[:-1])     # [row j, col i]
    X1, Y1 = np.meshgrid(e[1:], e[1:])
    om = A(X0, Y0) - A(X0, Y1) - A(X1, Y0) + A(X1, Y1)
    SX, SY = np.meshgrid(c, c)
    Q = np.zeros((6, 8, 3))
    total = 0.0
    for f in range(6):
        F, U = FWD[f], UP[f]
        R = np.cross(F, U)
        d = F[None, None, :] + SX[..., None] * R[None, None, :] + SY[..., None] * U[None, None, :]
        d /= np.linalg.norm(d, axis=2, keepdims=True)
        o = (d[..., 0] < 0).astype(int) | ((d[..., 1] < 0).astype(int) << 1) | ((d[..., 2] < 0).astype(int) << 2)
        L = px[f]                              # [row from the bottom j, texel i, rgb]
        total += om.sum()
        for a in range(6):
            cosA = np.clip(d @ AXES[a], 0.0, None)
            w = om * cosA
            for oc in range(8):
                m = o == oc
                Q[a, oc] += (w[m][:, None] * L[m]).sum(0)
    return Q, total


def read_f32(path, per):
    b = open(path, 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    return np.frombuffer(b, '<f4', n * per, 4).reshape(n, per).astype(np.float64)


def close_rows(a, b, rel, ab):
    return np.all(np.abs(a - b) <= ab + rel * np.maximum(np.abs(a), np.abs(b)), axis=tuple(range(1, a.ndim)))


def mode_check(run, dump):
    d = os.path.join(run, dump)
    out, bad = [], 0
    if not os.path.exists(os.path.join(d, 'gi_skypic.bin')):
        print('F FAIL the viewer dumped no gi_skypic.bin in %s (no drawn sky reached the probes)' % dump)
        print('skypic FAIL')
        return 1
    n, px = read_pic(d)
    faces, qv, seen = read_txt(d)
    okF = len(faces) == 6 and all(np.allclose(faces[f], np.stack([FWD[f], np.cross(FWD[f], UP[f]), UP[f]]), atol=1e-6) for f in range(6))
    out.append('F %s faces: %d read, the cube this file assumes' % ('PASS' if okF else 'FAIL', len(faces)))
    bad += not okF
    Q, total = own_q(n, px)
    big = max(float(np.abs(Q).max()), 1e-12)
    dq = float(np.abs(Q - qv).max()) / big
    okQ = seen == 48 and dq <= 1e-4 and abs(total - 4 * math.pi) < 1e-6
    out.append('Q %s the table: %dx%d a face, the solid angles sum to 4 pi x %.8f; largest difference %.2e of the '
               'largest entry (bar 1e-4); open sky up (+Z) %.4f %.4f %.4f'
               % ('PASS' if okQ else 'FAIL', n, n, total / (4 * math.pi), dq, *Q[4].sum(0)))
    bad += not okQ
    tb = os.path.join(run, 'bake')
    tbks = [read_tbk(os.path.join(tb, f)) for f in sorted(os.listdir(tb)) if f.endswith('.tbk')]
    own = []
    for t in tbks:
        vis = np.clip(t['probes']['sky'].astype(np.float64), 0, 1)
        tn = t['pext']['skytint'].astype(np.float64) / 255.0
        own.append(np.einsum('po,poc,aoc->pac', vis, tn, Q))
    own = np.concatenate(own)
    V = read_f32(os.path.join(d, 'gi_sky.bin'), 18).reshape(-1, 6, 3)
    if len(V) != len(own):
        out.append('S FAIL the probes: the bake holds %d, gi_sky.bin %d' % (len(own), len(V)))
        bad += 1
    else:
        good = close_rows(V, own, 0.01, 1e-4)
        sees = own.max((1, 2)) > 1e-6
        ratio = float(V.sum() / max(own.sum(), 1e-9))
        okS = good.mean() >= 0.99 and sees.sum() >= 20 and 0.99 <= ratio <= 1.01
        out.append('S %s the probes: %d, %d see sky; agree %.2f%% (each axis within 1%%), the viewer\'s total over '
                   'this file\'s %.5f' % ('PASS' if okS else 'FAIL', len(own), int(sees.sum()), 100 * good.mean(), ratio))
        bad += not okS
    print('\n'.join(out))
    print('skypic %s' % ('PASS' if bad == 0 else 'FAIL'))
    return 0 if bad == 0 else 1


def mode_flat(run, a, b):
    A = read_f32(os.path.join(run, a, 'gi_sky.bin'), 18)
    B = read_f32(os.path.join(run, b, 'gi_sky.bin'), 18)
    if A.shape != B.shape:
        print('K FAIL the two dumps hold %d and %d probes' % (len(A), len(B)))
        return 1
    big = max(float(np.abs(A).max()), 1e-12)
    d = float(np.abs(A - B).max()) / big
    ratio = float(A.sum() / max(B.sum(), 1e-12))
    ok = d <= 1e-3 and big > 1e-4
    print('K %s flat sky: %d probes; drawn+integrated against SKY1\'s flat: largest difference %.2e of the largest '
          'value (bar 1e-3), totals %.6f' % ('PASS' if ok else 'FAIL', len(A), d, ratio))
    return 0 if ok else 1


def enc(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - 0.055) * 255.0


def mode_zenith(shot, run, dump):
    from PIL import Image
    img = np.asarray(Image.open(shot).convert('RGB'), float)
    h, w = img.shape[:2]
    s = img[h // 2 - 10:h // 2 + 11, w // 2 - 10:w // 2 + 11].reshape(-1, 3).mean(0)
    n, px = read_pic(os.path.join(run, dump))
    k = max(1, n // 32)
    up = px[4, n // 2 - k:n // 2 + k, n // 2 - k:n // 2 + k].reshape(-1, 3).mean(0)
    dn = px[5, n // 2 - k:n // 2 + k, n // 2 - k:n // 2 + k].reshape(-1, 3).mean(0)
    eu, ed = enc(up), enc(dn)
    du, dd = float(np.abs(eu - s).max()), float(np.abs(ed - s).max())
    ok = du <= 3.0 and dd > 10.0
    print('D %s zenith: the drawn view %.1f %.1f %.1f; the capture\'s +Z %.1f %.1f %.1f (off by %.1f levels, bar 3); '
          'its -Z %.1f %.1f %.1f (off by %.1f, must be over 10)'
          % ('PASS' if ok else 'FAIL', *s, *eu, du, *ed, dd))
    return 0 if ok else 1


if __name__ == '__main__':
    m = sys.argv[1]
    if m == 'check':
        sys.exit(mode_check(sys.argv[2], sys.argv[3]))
    if m == 'flat':
        sys.exit(mode_flat(sys.argv[2], sys.argv[3], sys.argv[4]))
    if m == 'zenith':
        sys.exit(mode_zenith(sys.argv[2], sys.argv[3], sys.argv[4]))
    sys.exit('mode?')
