"""THE GAME'S TEMPORAL AA, REBUILT IN NUMPY (lane MOTION1, 2026-10-04; src/gl/gametaa.h, res/shaders/game_taa.frag).

Reads one frame's dump (WW_TAA_DUMP=<dir>: cur/hist_in/hist_out/out/mv/depth .bin + taa.txt) and does the
resolve again from the game's law as the lane notes trace it -- not from the GLSL -- with its own constants:

  K  constants  every number the viewer used, recomputed here from the trace: n = (frame & 7) + 1,
                offX = (2 Halton(2, n) - 1) / W, offY = (2 Halton(3, n) - 1) / H, the tent weights c2,
                the tap step c3 (y turned over: v is up here), c4 = the game's four setting defaults
  J  jitter     the projection the frame drew with (proj_uploaded) is the unjittered one with
                clip.xy += (offX, offY) * clip.w, offsets from THIS file's Halton -- and nothing else moved
  V  vectors    the motion vectors = the camera's reprojection of the dumped depth (reproj, double here)
  R  resolve    the picture (8-bit) and the next history (luma, motion length) rebuilt per pixel
  C  clamp      the same agreement over the pixels where the neighbourhood clamp moves the history luma by
                more than 2/255 while the history carries weight (at least 100 such pixels, else REFUSED)

Bars (written before the first run; the tolerance is the hardware's, not the law's): R and C need >= 99.9%
of pixels within 1/255 per channel (the picture) and within 2e-3 (the history luma, half floats). K and J
need every number within 1e-6 relative. V needs 99.9% of pixels within 1e-4 texture units.

RED CONTROLS: WW_TAA_RED=noclamp must fail C (and R); WW_TAA_RED=wrongjitter must fail J.

usage: python motion1_taa_check.py <dump dir> [--expect-red noclamp|wrongjitter]
"""
import math, os, sys

import numpy as np

# the game's settings defaults, read from the exe (lane notes: fTAALowFreq, fTAAHighFreq, fTAAPostSharpen,
# fTAAPostOverlay, fTAASharpen:Display)
LOW, HIGH, POST_SHARPEN, POST_OVERLAY, SHARPEN = 0.5, 0.8, 0.21, 0.21, 1.0
f32 = np.float32


def halton(base, i):
    """BSGraphics' Halton (float): r += fmod(i, base) * f; i = floor(i / base) via * (1/base); f *= 1/base"""
    inv = f32(1.0) / f32(base)
    f, r, i = inv, f32(0.0), f32(i)
    while i > 0:
        r = f32(r + f32(math.fmod(i, base)) * f)
        i = f32(math.floor(f32(inv * i)))
        f = f32(f * inv)
    return r


def tent(t):
    u = f32((f32(SHARPEN) * f32(t) + f32(1.0)) * f32(0.5))
    return f32(1.0) - abs(f32(0.5) - u) * f32(2.0) if 0.0 <= u <= 1.0 else f32(0.0)


def load_bin(path):
    raw = open(path, 'rb').read()
    w, h, ch = np.frombuffer(raw[:12], dtype='<i4')
    a = np.frombuffer(raw[12:], dtype='<f4').reshape(h, w, ch)  # row 0 = the bottom (GL)
    return a.copy()


def load_txt(path):
    d = {}
    for line in open(path, encoding='utf-8'):
        p = line.split()
        if not p:
            continue
        d[p[0]] = p[1:]
    return d


def main():
    args = sys.argv[1:]
    red = None
    if '--expect-red' in args:
        red = args[args.index('--expect-red') + 1]
    D = args[0]
    t = load_txt(os.path.join(D, 'taa.txt'))
    frame, n, W, H = int(t['frame'][0]), int(t['n'][0]), int(t['W'][0]), int(t['H'][0])
    fl = lambda k: np.array([float(x) for x in t[k]], dtype=np.float64)
    res = {}
    print('dump %s: frame %d n %d %dx%d reset %s red %s' % (D, frame, n, W, H, t['reset'][0], t['red'][0]))

    # ---- K: the constants ----
    nn = (frame & 7) + 1
    offX = f32(f32(halton(2, nn) * f32(2.0) - f32(1.0)) / f32(W))
    offY = f32(f32(halton(3, nn) * f32(2.0) - f32(1.0)) / f32(H))
    px, py = f32(offX * f32(0.5) * f32(W)), f32(offY * f32(0.5) * f32(H))
    sx = -1.0 if math.ceil(offX) > 0.5 else 1.0
    sy = -1.0 if math.ceil(offY) > 0.5 else 1.0
    tx0, ty0, tx1, ty1 = tent(px), tent(py), tent(px + f32(sx)), tent(py + f32(sy))
    w4 = np.array([ty0 * tx0, ty1 * tx0, ty0 * tx1, ty1 * tx1], dtype=np.float64)
    c2 = w4 / w4.sum()
    c3 = np.array([sx / W, -sy / H, 0, 0])
    c4 = np.array([LOW, HIGH, POST_SHARPEN, POST_OVERLAY])
    c0 = np.array([1.0 / W, 1.0 / H, 1.0, 1.0])
    c5 = np.array([1.0 / W, 1.0 / H, 1.0, 1.0])
    rel = lambda a, b: float(np.max(np.abs(np.asarray(a) - np.asarray(b)) / np.maximum(np.abs(np.asarray(b)), 1e-12)))
    worstK = max(abs(n - nn) * 1.0, rel(fl('offX')[0], offX), rel(fl('offY')[0], offY), rel(fl('c0'), c0),
                 rel(fl('c2'), c2), rel(fl('c3'), c3), rel(fl('c4'), c4), rel(fl('c5'), c5))
    res['K'] = worstK <= 1e-6
    print('K %s  n %d (expect %d)  off (%.9g, %.9g) expect (%.9g, %.9g)  worst rel %.3g' % (
        'PASS' if res['K'] else 'FAIL', n, nn, fl('offX')[0], fl('offY')[0], offX, offY, worstK))

    # ---- J: the jitter the frame drew with ----
    PU = fl('proj_unjittered').reshape(4, 4)  # rows of this array = columns of the matrix (column-major)
    PJ = fl('proj_uploaded').reshape(4, 4)
    exp = PU.copy()
    for c in range(4):
        exp[c, 0] += float(offX) * PU[c, 3]
        exp[c, 1] += float(offY) * PU[c, 3]
    dJ = float(np.max(np.abs(PJ - exp) / np.maximum(np.abs(exp), 1e-6)))
    moved = float(np.max(np.abs(PJ - PU)))
    res['J'] = dJ <= 1e-6 and moved > 0
    print('J %s  uploaded vs unjittered x Halton jitter: worst rel %.3g (the jitter moved the matrix by %.3g)' % (
        'PASS' if res['J'] else 'FAIL', dJ, moved))

    # ---- inputs ----
    cur = load_bin(os.path.join(D, 'cur.bin'))[..., :3].astype(np.float64)
    hin = load_bin(os.path.join(D, 'hist_in.bin')).astype(np.float64)
    hout = load_bin(os.path.join(D, 'hist_out.bin')).astype(np.float64)
    out = load_bin(os.path.join(D, 'out.bin'))[..., :3].astype(np.float64)
    mv = load_bin(os.path.join(D, 'mv.bin')).astype(np.float64)
    dep = load_bin(os.path.join(D, 'depth.bin'))[..., 0].astype(np.float64)
    jj, ii = np.mgrid[0:H, 0:W]

    # ---- V: the motion vectors from the depth ----
    RP = fl('reproj').reshape(4, 4).T  # to row-major
    u = (ii + 0.5) / W
    v = (jj + 0.5) / H
    ndc = np.stack([u * 2 - 1 - float(offX), v * 2 - 1 - float(offY), dep * 2 - 1, np.ones_like(u)], -1)
    p = ndc @ RP.T
    mvx = (p[..., 0] / p[..., 3] - ndc[..., 0]) * 0.5
    mvy = (p[..., 1] / p[..., 3] - ndc[..., 1]) * 0.5
    errV = np.maximum(np.abs(mvx - mv[..., 0]), np.abs(mvy - mv[..., 1]))
    agreeV = float(np.mean(errV <= 1e-4))
    res['V'] = agreeV >= 0.999
    print('V %s  motion vectors within 1e-4 on %.4f%% (max |mv| %.4g texture units)' % (
        'PASS' if res['V'] else 'FAIL', 100 * agreeV, float(np.max(np.hypot(mv[..., 0], mv[..., 1])))))

    # ---- R: the resolve ----
    def tap(img, dx, dy):  # NEAREST, CLAMP, one step of c3 = (sx, -sy) texels
        x = np.clip(ii + int(dx * sx), 0, W - 1)
        y = np.clip(jj + int(dy * -sy), 0, H - 1)
        return img[y, x]
    offs = dict(A=(-1, -1), B=(1, 1), C=(1, 0), D=(1, -1), E=(-1, 1), F=(0, -1), G=(0, 1), H=(-1, 0), Z=(0, 0))
    d = {k: tap(dep, *o) for k, o in offs.items()}
    # the closest depth; ties broken in the game's order (lane notes: (A,B,D), (F,C), (H,E), (Z,G))
    m = np.minimum(np.minimum(d['B'], d['D']), d['A'])
    sel = np.where(m == d['A'], 0, 1)
    names = 'ABCDEFGHZ'
    sel = np.where(m == d['D'], names.index('D'), np.where(sel == 0, names.index('A'), names.index('B')))
    m = np.minimum(m, d['F']); m = np.minimum(m, d['C'])
    sel = np.where(m == d['C'], names.index('C'), sel); sel = np.where(m == d['F'], names.index('F'), sel)
    m = np.minimum(m, d['H']); m = np.minimum(m, d['E'])
    sel = np.where(m == d['E'], names.index('E'), sel); sel = np.where(m == d['H'], names.index('H'), sel)
    m = np.minimum(m, d['Z']); m = np.minimum(m, d['G'])
    sel = np.where(m == d['G'], names.index('G'), sel); sel = np.where(m == d['Z'], names.index('Z'), sel)
    mvs = np.zeros((H, W, 2))
    for k, o in offs.items():
        mvs = np.where((sel == names.index(k))[..., None], tap(mv, *o), mvs)
    mvLen = np.hypot(mvs[..., 0], mvs[..., 1])
    hu = u + mvs[..., 0]
    hv = v + mvs[..., 1]
    # BILERP, CLAMP
    xs, ys = hu * W - 0.5, hv * H - 0.5
    x0, y0 = np.floor(xs), np.floor(ys)
    ax, ay = xs - x0, ys - y0
    X0, X1 = np.clip(x0, 0, W - 1).astype(int), np.clip(x0 + 1, 0, W - 1).astype(int)
    Y0, Y1 = np.clip(y0, 0, H - 1).astype(int), np.clip(y0 + 1, 0, H - 1).astype(int)
    h = (hin[Y0, X0] * ((1 - ax) * (1 - ay))[..., None] + hin[Y0, X1] * (ax * (1 - ay))[..., None]
         + hin[Y1, X0] * ((1 - ax) * ay)[..., None] + hin[Y1, X1] * (ax * ay)[..., None])
    Yh, vh = h[..., 0], h[..., 2]
    col = {k: tap(cur, *o) for k, o in offs.items()}
    Y = {k: 0.5 * c[..., 1] + 0.25 * c[..., 0] + 0.25 * c[..., 2] for k, c in col.items()}
    R = col['Z'] * c2[0] + col['G'] * c2[1] + col['H'] * c2[2] + col['E'] * c2[3]
    # the bracket: upper = the darkest at or above the history's luma, lower = the brightest below it,
    # the centre first, then G, H, E, F, C, D, A, B; none = 1.001 / -0.001
    upY = np.full((H, W), 1.001); upC = np.full((H, W, 3), 1.001)
    loY = np.full((H, W), -0.001); loC = np.full((H, W, 3), -0.001)
    for k in 'ZGHEFCDAB':
        y = Y[k]
        above = y >= Yh
        take_up = above & (y < upY) if k != 'Z' else above & (y < 1.001)
        upY = np.where(take_up, y, upY); upC = np.where(take_up[..., None], col[k], upC)
        take_lo = (~above) & (y > loY) if k != 'Z' else (~above) & (y > -0.001)
        loY = np.where(take_lo, y, loY); loC = np.where(take_lo[..., None], col[k], loC)
    noUp, noLo = upY > 1.0, loY < 0.0
    loY2 = np.where(noLo, upY, loY); loC2 = np.where(noLo[..., None], upC, loC)
    upY2 = np.where(noUp, loY2, upY); upC2 = np.where(noUp[..., None], loC2, upC)
    YhC = np.minimum(upY2, np.maximum(loY2, Yh))
    offscreen = (hu >= 1.0) | (hv >= 1.0) | (np.minimum(hu, hv) <= 0.0)
    YZ, cZ = Y['Z'], col['Z']
    YhP = np.where(offscreen, YZ, YhC)
    vhP = np.where(offscreen, 0.0, vh)
    spanY = upY2 - loY2
    with np.errstate(divide='ignore', invalid='ignore'):
        tt = np.where(spanY > 0.01, (YhC - loY2) / np.where(spanY > 0.01, spanY, 1.0), 0.5)
    dY = YhP - YZ
    vlen = np.clip(mvLen / (c0[0] * 128.0), 0, 1)
    wf = vlen * (c4[0] - c4[1]) + c4[1]
    k = np.maximum(1.0 - np.abs(vlen - vhP) * 20.0, 0.0)
    w = np.minimum(k, wf)
    Yn = np.where(np.abs(dY * w) < 0.01, YZ, w * dY + YZ)
    histC = loC2 + tt[..., None] * (upC2 - loC2)
    histC = np.where(offscreen[..., None], cZ, histC)
    Rr = np.where(offscreen[..., None], cZ, R)
    curC = k[..., None] * (cZ - Rr) + Rr
    o1 = np.clip(w[..., None] * (histC - curC) + curC, 0, 1)
    o1 = np.clip((o1 - Rr) * c4[2] + o1, 0, 1)
    o1 = np.clip(c4[3] * (Rr - o1) + o1, 0, 1)
    errO = np.max(np.abs(np.round(o1 * 255) - np.round(out * 255)), -1)
    errY = np.abs(np.clip(Yn, 0, 1) - hout[..., 0])
    errL = np.abs(vlen - hout[..., 2])
    okPix = (errO <= 1) & (errY <= 2e-3) & (errL <= 2e-3)
    agreeR = float(np.mean(okPix))
    res['R'] = agreeR >= 0.999
    print('R %s  picture+history agree on %.4f%% of %d pixels (picture within 1/255 %.4f%%, history luma %.4f%%, '
          'motion length %.4f%%); offscreen %.2f%%, mean history weight %.3f' % (
              'PASS' if res['R'] else 'FAIL', 100 * agreeR, W * H, 100 * np.mean(errO <= 1),
              100 * np.mean(errY <= 2e-3), 100 * np.mean(errL <= 2e-3), 100 * np.mean(offscreen), float(np.mean(w))))

    # ---- C: where the clamp acts ----
    acts = (~offscreen) & (np.abs(YhC - Yh) > 2.0 / 255) & (w > 0.05)
    nC = int(acts.sum())
    if nC < 100:
        res['C'] = False
        print('C FAIL  REFUSED: only %d pixels where the clamp acts (need 100)' % nC)
    else:
        agreeC = float(np.mean(okPix[acts]))
        res['C'] = agreeC >= 0.999
        print('C %s  where the clamp moves the history luma (%d pixels, %.2f%%): agree on %.4f%%' % (
            'PASS' if res['C'] else 'FAIL', nC, 100.0 * nC / (W * H), 100 * agreeC))

    allok = all(res.values())
    if red:
        must = {'noclamp': 'C', 'wrongjitter': 'J'}[red]
        print('taa RED %s: stage %s %s' % (red, must, 'FAILS as it must' if not res[must] else 'PASSED (the gate is blind)'))
        print('taa %s' % ('FAIL' if not res[must] else 'BLIND'))
        return 0 if not res[must] else 1
    print('taa %s' % ('PASS' if allok else 'FAIL'))
    return 0 if allok else 1


if __name__ == '__main__':
    sys.exit(main())
