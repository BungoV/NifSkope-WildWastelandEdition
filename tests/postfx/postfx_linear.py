"""lane POSTFX2: offline measure of the game's depth of field and motion blur on the LINEAR frame before the tone map
(row WW_CELL_POST_LINEAR, src/gl/cellpost.cpp wwCellPostApplyLinear) against the shipped order (after the tone map).
The REAL shaders (game_mblur.frag, game_dofmblur.frag) through the real driver in a hidden GL 4.1 context; the tone
map is cell_hdr.frag's vanilla curve (cellCurveW, E = 0) then 1/2.2, in numpy.

    python -I tests/postfx/postfx_linear.py [--red display] [--ms]

Picture: a dark street (0.03), a grey wall (0.18), a sky (0.6) and three lamps (3x3 px at 30, far past white).
  display order: T(frame) in RGBA16F -> blur -> picture (what ships)
  linear order:  frame in RGBA32F -> blur -> T (the row)
Gates (exit 0 only when every gate passes):
  still          no motion, no depth of field: linear order == display order, max 0 (the row moves nothing still)
  mb energy      a 1/60 s pan: the lamps' light in the streaks over their light before the blur (linear units,
                 display picture through the curve's inverse): linear order 0.9..1.1; the display order must lose
                 more than half (the measured change; the red runs the display order in the linear slot: fails)
  dof energy     the lamps out of focus (far): the same ratio through the depth of field
  shift          the share of pixels >= 1 step apart between the orders, away from the lamps (printed, no bar)
  --ms           1080p GPU ms of the blur on an RGBA32F frame (the linear frame's own format) vs RGBA16F
"""
import os, sys
import numpy as np

TREE = os.environ.get('TREE', os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')))
sys.path.insert(0, TREE + '/tests/postfx')
import postfx_fuse as F   # noqa: E402

P = F.P
RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
RGBA16F, RGBA32F, RG16F, R32F = 0x881A, 0x8814, 0x822F, 0x822E
LINEAR, NEAREST = P.LINEAR, P.NEAREST


def curve(v):
    """cell_hdr.frag cellCurveW (vanilla, E = 0) of 2 x the light, then 1/2.2: display value 0..1"""
    x = 2.0 * np.maximum(v, 0.0)
    c = x * (0.15 * x + 0.05) / (x * (0.15 * x + 0.5) + 0.06)
    return np.clip(c / (19.376 * 0.040856), 0.0, 1.0) ** (1.0 / 2.2)


def curve_inv(d):
    """the light a display value came from (numeric inverse; display 1 reads as the curve's white point)"""
    grid = np.concatenate([np.linspace(0, 1, 4001), np.geomspace(1.0001, 400, 4000)])
    c = curve(grid)
    keep = np.concatenate([[True], np.diff(c) > 0])   # the rising part only (the curve clips at white)
    return np.interp(d, c[keep], grid[keep])


def scene(w, h):
    yy, xx = np.mgrid[0:h, 0:w]
    col = np.full((h, w, 4), 0.03); col[..., 3] = 1
    col[:int(0.35 * h)] = (0.6, 0.65, 0.75, 1)
    col[int(0.35 * h):int(0.7 * h), int(0.55 * w):int(0.85 * w), :3] = 0.18
    lamps = []
    for fx, fy in ((0.2, 0.5), (0.4, 0.45), (0.7, 0.3)):
        x0, y0 = int(fx * w), int(fy * h)
        col[y0 - 1:y0 + 2, x0 - 1:x0 + 2, :3] = 30.0
        lamps.append((x0, y0))
    dist = np.where(yy < 0.35 * h, 1e6, 2000.0)   # everything far: the depth of field blurs it all
    depth = F.window_depth(np.minimum(dist, 9999.0)); depth[yy < 0.35 * h] = 0.9999
    return col, depth, lamps


def blur4(img):
    w4, h4 = (img.shape[1] + 3) // 4, (img.shape[0] + 3) // 4
    pad = np.zeros((h4 * 4, w4 * 4, 4)); pad[:img.shape[0], :img.shape[1]] = img
    q = pad.reshape(h4, 4, w4, 4, 4).mean(axis=(1, 3))
    k = np.ones(5) / 5   # the game's Blur passes, a 5-tap box per axis (any blur: both orders take the same)
    q = np.apply_along_axis(lambda r: np.convolve(np.pad(r, 2, mode='edge'), k, 'valid'), 0, q)
    return np.apply_along_axis(lambda r: np.convolve(np.pad(r, 2, mode='edge'), k, 'valid'), 1, q)


class Lin(F.Fuse):
    def chain(self, w, h, frame, depth, mv, dof, mb, fmt, timed=False):
        """the shipped pass on one frame in the given format: game_dofmblur when dof is set, else game_mblur"""
        tc = self.tex(w, h, fmt, LINEAR, frame)
        tv = self.tex(w, h, RG16F, LINEAR, mv)
        out = self.tex(w, h, fmt, LINEAR)
        if dof is not None:
            tb = self.tex((w + 3) // 4, (h + 3) // 4, RGBA16F, LINEAR, blur4(frame))   # the game's blur targets: RGBA16F
            td = self.tex(w, h, R32F, NEAREST, depth[..., None])
        times = []
        for _ in range(32 if timed else 1):
            if timed:
                self.bq(0x88BF, self.q)
            if dof is None:
                self.pass_('game_mblur', out, w, h, (('t0', tc), ('t1', tv)), {'c0': mb[0], 'c1': mb[1]})
            else:
                self.pass_('game_dofmblur', out, w, h, (('t0', tc), ('t1', tv), ('t2', tb), ('t3', td)),
                           {'dc0': dof[0], 'dc1': dof[1], 'dc2': dof[2], 'dc3': dof[3], 'mc0': mb[0], 'mc1': mb[1],
                            'farOnly': 0, 'dofRed': 0})
            if timed:
                self.eq(0x88BF)
                import ctypes
                ns = ctypes.c_uint64(); self.gq(self.q, 0x8866, ctypes.byref(ns)); times.append(ns.value / 1e6)
        o = None if timed else self.read(out, w, h)
        self.free()
        return float(np.median(times[2:])) if timed else o

    def orders(self, w, h, col, depth, mv, dof, mb):
        tm = np.dstack([curve(col[..., :3]), col[..., 3:]])
        disp = self.chain(w, h, tm, depth, mv, dof, mb, RGBA16F)[..., :3]
        if RED == 'display':   # the display order in the linear slot (its light read back through the curve)
            raw = curve_inv(disp)
        else:
            raw = self.chain(w, h, col, depth, mv, dof, mb, RGBA32F)[..., :3]
        return disp, curve(raw), raw


def main():
    R = Lin(P.SD)
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        fails += 0 if ok else 1
        print('%-12s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)

    w, h = 640, 360
    col, depth, lamps = scene(w, h)
    still_mb = ((50 * 0.001 * 60.0, 0.01, 70.0, 0.0), (1.0, 1.0, (w - 1.0) / w, (h - 1.0) / h))
    d, l, _ = R.orders(w, h, col, depth, np.zeros((h, w, 2)), None, still_mb)
    e = np.abs(curve(col[..., :3].astype(np.float32)) - l).max() * 255   # the linear stage copies its frame through untouched
    gate('still', e < 0.001, 'linear order vs the unblurred picture: max %.2e steps (bar 0.001: float rounding of '
         'the curve, no blur)' % e)

    near = np.zeros((h, w), bool)
    for x0, y0 in lamps:
        near[max(0, y0 - 20):y0 + 21, max(0, x0 - 40):x0 + 41] = True
    lamp_light = sum(float(col[y0 - 1:y0 + 2, x0 - 1:x0 + 2, :3].sum()) - 9 * 3 * 0.03 for x0, y0 in lamps)

    bg = col[..., :3].copy()
    for x0, y0 in lamps:
        bg[y0 - 1:y0 + 2, x0 - 1:x0 + 2] = 0.03

    def energy(light):
        """the light above the street in the lamps' boxes, over the lamps' own"""
        return float((light - bg).clip(0)[near].sum()) / lamp_light

    def lit(pic):
        """pixels the lamps lift by >= 0.1 of display white"""
        return int(((pic - curve(bg)).max(axis=2) >= 0.1)[near].sum())

    mv = np.dstack([np.full((h, w), -0.03), np.zeros((h, w))])   # a pan: ~19 px streaks at 1/60 s
    D = 50.0
    dof = ((D, D, D, 10000.0), (1.0, 1.0, 1.0, 0.0), (-1e8, F.ZN, F.ZF - F.ZN, F.ZF * F.ZN), (1.0, 1.0, 1.0, 1.0))
    pics = []
    for name, m, dd in (('mb', mv, None), ('dof', np.zeros((h, w, 2)), dof)):
        d, l, raw = R.orders(w, h, col, depth, m, dd, still_mb)
        rl, rd = energy(raw), energy(curve_inv(d))
        gate(name + ' energy', 0.9 <= rl <= 1.1 and rd < 0.5,
             'lamp light kept: linear order %.3f (bar 0.9..1.1), display order %.3f (< 0.5); pixels the lamps lift '
             'by >= 0.1: %d vs %d%s' % (rl, rd, lit(l), lit(d), ' (RED: display)' if RED else ''))
        sh = (np.abs(d - l).max(axis=2) * 255 >= 1.0)[~near].mean() * 100
        print('shift %-6s %.3f%% of pixels away from the lamps >= 1 step apart' % (name, sh))
        pics += [d, l]
    np.save(os.path.join(os.environ.get('TMPDIR', '.'), 'lin_pics.npy'), np.stack(pics))

    if '--ms' in sys.argv:
        W, H = 1920, 1080
        c2, d2, _ = scene(W, H)
        mv2 = np.dstack([np.full((H, W), -0.03), np.zeros((H, W))])
        mb2 = ((50 * 0.001 * 60.0, 0.01, 70.0, 0.0), (1.0, 1.0, (W - 1.0) / W, (H - 1.0) / H))
        for name, dd in (('mb', None), ('dof+mb', dof)):
            t16 = R.chain(W, H, c2, d2, mv2, dd, mb2, RGBA16F, timed=True)
            t32 = R.chain(W, H, c2, d2, mv2, dd, mb2, RGBA32F, timed=True)
            print('ms %-8s RGBA16F %.3f ms, RGBA32F %.3f ms (1080p, the pass alone)' % (name, t16, t32))
    print('POSTLINEAR RESULT %s (%d fail)%s' % ('PASS' if not fails else 'FAIL', fails, ' red=' + RED if RED else ''))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
