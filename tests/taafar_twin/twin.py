"""lane FARTOGGLE3: does the temporal AA make a sub-pixel far object blink on a STILL camera?

The REAL resolves (res/shaders/game_taa.frag, taa2.frag + its final pass) through the real driver in a hidden GL 4.1
context (tests/spells/shader_link_check.py), no NifSkope window. The frames are a synthetic far view drawn the way
the cell view draws them:
  * the TAA jitter exactly as gametaa.cpp: frame k = 1, 2, ..., n = (k & 7) + 1, Halton (2, 3), offX = (h * 2 - 1) / W;
    the projection moves the content BY the jitter (pixel p sees the scene at p - jitterPx);
  * the view's MSAA (bungo's saved setting: 4x, the standard 4x pattern) resolved by a box average, then 8-bit
    (the tone-mapped RGBA8 frame the resolve reads); the depth of sample 0 (a multisampled depth blit's choice);
  * depth = the cell view's perspective (near 16, far 472098: glview with a far field), sky at 1.0;
  * a ground below the horizon (its distance grows toward the horizon, a smooth texture) and a sky gradient above;
  * far objects: squares 0.3 .. 1.4 px across at random sub-pixel phases, dark and bright, standing on the ground
    (ground behind them farther) and on the skyline (sky behind), none closer than 6 px to another.
Per object and frame the after-resolve visibility v = sum over its 5x5 of (Y_out - Y_out of the same run without
objects) / (Y_object - Y_ground/sky there), divided by the object's area (1.0 = shown at its true weight).
Over the steady frames 32..95 (8 jitter cycles):
  blink    an object PERCEPTIBLE in its best frame (|v| * area * contrast >= 0.04 luma px) whose worst frame keeps
           less than half of its best (min v < 0.5 max v): it shows and goes, the "toggles on and off"
  flicker  mean |v_f - v_f-1| / mean v over the perceptible objects
  kept     median of mean v (the object's average weight; the ideal is 1.0; TAA off at 4x MSAA is the plain frame)
Moving case: the camera pans 0.5 px/frame with its vectors (the same objects): blink/flicker/kept again, and the
ghost of a dark 12 px bar moving 2 px/frame with NO vectors in front of the ground (|dY| in the 8 px behind it).
VERDICT PASS (for --engine) = still blink 0, still flicker <= 0.15, moving flicker <= 0.25, ghost <= the bar given
(--ghost-bar, default: the same engine with the resolve as on origin/main, i.e. --shaders of the base) + 0.002.

python -I twin.py [--shaders DIR] [--engines off,game,1,2,3,4] [--msaa 4] [--red N] [--frames 80] [--seed 7]
          [--verdict ENGINE] [--ghost-bar X] [--png DIR]
"""
import argparse, ctypes, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
import shader_link_check as S   # noqa: E402
import atmo_lut_check as C      # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(TREE, 'res', 'shaders'))
ap.add_argument('--engines', default='off,game,1,2,3,4')
ap.add_argument('--msaa', type=int, default=4, choices=(1, 4))
ap.add_argument('--red', type=int, default=0, help='taa2Red bits for every taa2 engine')
ap.add_argument('--frames', type=int, default=96)
ap.add_argument('--seed', type=int, default=7)
ap.add_argument('--verdict', default='', help='engine the VERDICT line judges (e.g. 2)')
ap.add_argument('--ghost-bar', type=float, default=-1.0)
ap.add_argument('--png', default='')
a = ap.parse_args()

gl = S.gl
GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
W, H, HOR = 320, 192, 96
NEAR, FAR = 16.0, 472098.0
F0 = 32
T_VIS = 0.04
rng = np.random.default_rng(a.seed)
MS4 = np.array([(-0.125, 0.375), (0.375, 0.125), (-0.375, -0.125), (0.125, -0.375)])   # D3D 4x, y turned up
MSP = MS4 if a.msaa == 4 else np.zeros((1, 2))


def halton(b, i):
    f, r = 1.0 / b, 0.0
    while i > 0:
        r += (i % b) * f
        i = math.floor(i / b)
        f /= b
    return r


def tent(t):
    u = (1.0 * t + 1) * 0.5
    return 0.0 if u < 0 or u > 1 else 1 - abs(0.5 - u) * 2


def depth_of(z):
    return (FAR / (FAR - NEAR)) * (1.0 - NEAR / z)


# the ground's smooth texture: value noise on a 6 px lattice
_lat = rng.random((H // 6 + 3, W // 6 + 3))


def ground_tex(x, y):
    gx, gy = x / 6.0, y / 6.0
    ix, iy = np.floor(gx).astype(int), np.floor(gy).astype(int)
    fx, fy = gx - ix, gy - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    ix, iy = ix.clip(0, _lat.shape[1] - 2), iy.clip(0, _lat.shape[0] - 2)
    v = (_lat[iy, ix] * (1 - fx) + _lat[iy, ix + 1] * fx) * (1 - fy) + (_lat[iy + 1, ix] * (1 - fx) + _lat[iy + 1, ix + 1] * fx) * fy
    return v


GROUND = np.array([0.40, 0.36, 0.27])
SKY0, SKY1 = np.array([0.72, 0.80, 0.88]), np.array([0.55, 0.68, 0.85])
DARK, BRIGHT = np.array([0.12, 0.12, 0.10]), np.array([0.88, 0.86, 0.80])
SIZES = (0.3, 0.45, 0.6, 0.8, 1.0, 1.4)
KZ = 240000.0   # ground distance = KZ / (rows below the horizon)


def background(x, y):
    """colour (..., 3) and view distance z (inf = sky) at scene points x, y (GL px, y up)"""
    sky = y >= HOR
    t = np.clip((y - HOR) / (H - HOR), 0, 1)[..., None]
    col_sky = SKY0 * (1 - t) + SKY1 * t
    col_gr = GROUND * (0.85 + 0.3 * ground_tex(x, y))[..., None]
    col = np.where(sky[..., None], col_sky, col_gr)
    z = np.where(sky, np.inf, KZ / np.maximum(HOR - y, 1e-3))
    return col, z


def make_objects():
    objs = []
    rows_g = (62, 70, 78, 86)          # standing on the ground (z 7,000 .. 24,000)
    rows_s = (112, 128, 144, 160)      # on the skyline (sky behind), z 20,000 .. 60,000
    k = 0
    for cls, rows in (('ground', rows_g), ('sky', rows_s)):
        for r in rows:
            for c in range(36):
                x = 6 + c * 8.6 + rng.random()
                y = r + rng.random()
                s = SIZES[k % len(SIZES)]
                col = DARK if (k // len(SIZES)) % 2 == 0 else BRIGHT
                if cls == 'ground':
                    z = KZ / max(HOR - y, 1e-3) * 0.995   # its base on the ground, a hair in front
                else:
                    z = 20000.0 + 40000.0 * rng.random()
                objs.append(dict(cls=cls, x=x, y=y, s=s, col=col, z=z))
                k += 1
    return objs


OBJS = make_objects()
PX, PY = np.meshgrid(np.arange(W) + 0.5, np.arange(H) + 0.5)   # pixel centres, row 0 = bottom (GL)


def frame(jx, jy, pan, with_objs, bar=None, k=0):
    """the resolved 8-bit colour (H, W, 3) and depth (H, W) of one frame; the scene moved right by `pan` px"""
    acc = np.zeros((H, W, 3))
    dep = None
    for si, (ox, oy) in enumerate(MSP):
        sx = PX + ox - jx - pan
        sy = PY + oy - jy
        col, z = background(sx, sy)
        if with_objs:
            for o in OBJS:
                h = o['s'] / 2
                x0, y0 = int(o['x'] + pan) - 3, int(o['y']) - 3
                ys, xs = slice(max(y0, 0), min(y0 + 8, H)), slice(max(x0, 0), min(x0 + 8, W))
                m = (np.abs(sx[ys, xs] - o['x']) <= h) & (np.abs(sy[ys, xs] - o['y']) <= h) & (o['z'] < z[ys, xs])
                if m.any():
                    col[ys, xs][m] = o['col']
                    z[ys, xs][m] = o['z']
        if bar is not None:   # a dark bar moving on its own (no vectors), in front of everything below the horizon
            bx = PX + ox - jx
            m = (bx >= bar) & (bx < bar + 12) & (sy < HOR - 10) & (sy > 20)
            col[m] = 0.1
            z[m] = 3000.0
        acc += col
        if si == 0:
            dep = np.where(np.isinf(z), 1.0, depth_of(np.where(np.isinf(z), 1e9, z)))
    # the tone map's TPDF dither (ww_dither.glsl, seeded by the TAA frame; 0 with the TAA off), then 8 bits
    nz = np.random.default_rng(1000 + k)
    tp = (nz.random((H, W, 1)) + nz.random((H, W, 1)) - 1.0) / 255.0
    return np.round(np.clip(acc / len(MSP) + tp, 0, 1) * 255) / 255, dep


class Rig:
    def __init__(self, sdir):
        self.g = C.Gl()
        self.p = {k: C.build(sdir, k + '.prog') for k in ('game_taa', 'game_taa_copy', 'taa2', 'taa2_rcas')}
        self.texsub = S.fn('glTexSubImage2D', None, GLenum, GLint, GLint, GLint, ctypes.c_int, ctypes.c_int, GLenum, GLenum, ctypes.c_void_p)
        self.drawbufs = S.fn('glDrawBuffers', None, ctypes.c_int, ctypes.POINTER(GLenum))
        self.u4 = S.fn('glUniform4f', None, GLint, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_float)
        self.um4 = S.fn('glUniformMatrix4fv', None, GLint, ctypes.c_int, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_float))
        fb = GLuint(); self.g.genfb(1, ctypes.byref(fb)); self.fb = fb.value

    def tex(self, internal, filt, data=None):
        t = GLuint()
        gl.glGenTextures(1, ctypes.byref(t)); gl.glBindTexture(0x0DE1, t)
        buf = None if data is None else np.ascontiguousarray(data, np.float32).ctypes.data_as(ctypes.c_void_p)
        gl.glTexImage2D(0x0DE1, 0, internal, W, H, 0, 0x1908, 0x1406, buf)
        for k, v in ((0x2801, filt), (0x2800, filt), (0x2802, 0x812F), (0x2803, 0x812F)):
            gl.glTexParameteri(0x0DE1, k, v)
        return t.value

    def upload(self, t, arr):
        if arr.ndim == 2:
            arr = np.dstack([arr, arr, arr])
        rgba = np.concatenate([arr, np.ones(arr.shape[:2] + (1,))], 2)
        gl.glBindTexture(0x0DE1, t)
        self.texsub(0x0DE1, 0, 0, 0, W, H, 0x1908, 0x1406, np.ascontiguousarray(rgba, np.float32).ctypes.data_as(ctypes.c_void_p))

    def read(self, t):
        self.g.bindfb(0x8D40, self.fb)
        self.g.fbtex(0x8D40, 0x8CE0, 0x0DE1, t, 0)
        self.g.fbtex(0x8D40, 0x8CE1, 0x0DE1, 0, 0)
        self.g.fbtex(0x8D40, 0x8CE2, 0x0DE1, 0, 0)
        gl.glReadBuffer(0x8CE0)
        buf = (ctypes.c_float * (W * H * 4))()
        gl.glReadPixels(0, 0, W, H, 0x1908, 0x1406, buf)
        return np.frombuffer(buf, np.float32).reshape(H, W, 4)[..., :3].astype(np.float64)

    def bind(self, prog, texs):
        g = self.g
        g.use(prog)
        for unit, (name, t) in enumerate(texs):
            g.active(0x84C0 + unit); gl.glBindTexture(0x0DE1, t)
            l = g.loc(prog, name.encode())
            if l >= 0:
                g.u1i(l, unit)

    def targets(self, *ts):
        self.g.bindfb(0x8D40, self.fb)
        for i in range(3):
            self.g.fbtex(0x8D40, 0x8CE0 + i, 0x0DE1, ts[i] if i < len(ts) else 0, 0)
        n = len(ts)
        self.drawbufs(n, (GLenum * n)(*[0x8CE0 + i for i in range(n)]))

    def run(self, engine, frames, red=0):
        """engine 'game' or a taa2 tier; frames: list of (rgb, depth, pan speed px/frame). Yields the picture."""
        L = lambda prog, nm: self.g.loc(prog, nm.encode())
        cur = self.tex(0x8814, 0x2600)
        dep = self.tex(0x8814, 0x2600)
        hist = [self.tex(0x881A, 0x2601), self.tex(0x881A, 0x2601)]
        lock = [self.tex(0x881A, 0x2600, np.zeros((H, W, 4))), self.tex(0x881A, 0x2600, np.zeros((H, W, 4)))]
        out = self.tex(0x8058, 0x2600)
        fin = self.tex(0x8814, 0x2600)
        mv = self.tex(0x8814, 0x2600)
        z = np.zeros((H, W))
        for i, (rgb, d, speed) in enumerate(frames):
            k = i + 1
            n = (k & 7) + 1
            offX, offY = (halton(2, n) * 2 - 1) / W, (halton(3, n) * 2 - 1) / H
            jx, jy = offX * W / 2, offY * H / 2
            self.upload(cur, rgb)
            self.upload(dep, d)
            self.upload(mv, np.dstack([np.full((H, W), -speed / W), z, z]))
            # previous unjittered clip = current - the pan (column-major: translation in [12])
            rp = [1.0 if j % 5 == 0 else 0.0 for j in range(16)]
            rp[12] = -2.0 * speed / W
            reproj = (ctypes.c_float * 16)(*rp)
            rd, wr = k & 1 ^ 1, k & 1
            gl.glViewport(0, 0, W, H)
            if engine == 'game':
                p = self.p['game_taa']
                if i == 0:   # gametaa.cpp: a reset clears both histories to black
                    self.upload(hist[rd], np.zeros((H, W, 3)))
                px, py = offX * 0.5 * W, offY * 0.5 * H
                sx = -1.0 if math.ceil(offX) > 0.5 else 1.0
                sy = -1.0 if math.ceil(offY) > 0.5 else 1.0
                tx0, ty0, tx1, ty1 = tent(px), tent(py), tent(px + sx), tent(py + sy)
                wt = [ty0 * tx0, ty1 * tx0, ty0 * tx1, ty1 * tx1]
                s = sum(wt)
                self.targets(hist[wr], out)
                self.bind(p, (('curTex', cur), ('histTex', hist[rd]), ('mvTex', mv), ('depthTex', dep)))
                self.u4(L(p, 'c0'), 1 / W, 1 / H, 1, 1)
                self.u4(L(p, 'c2'), *[x / s for x in wt])
                self.u4(L(p, 'c3'), sx / W, -sy / H, 0, 0)
                self.u4(L(p, 'c4'), 0.5, 0.8, 0.21, 0.21)
                self.u4(L(p, 'c5'), 1 / W, 1 / H, 1, 1)
                self.g.u1i(L(p, 'taaRed'), 0); self.g.u1i(L(p, 'taaTap'), 0)
                self.g.u1i(L(p, 'wwDitherOn'), 0)
                gl.glDrawArrays(5, 0, 4)
                fp = self.p['game_taa_copy']
            else:
                tier = engine
                p = self.p['taa2']
                self.targets(hist[wr], out, lock[wr])
                self.bind(p, (('curTex', cur), ('histTex', hist[rd]), ('mvTex', mv), ('depthTex', dep), ('lockTex', lock[rd]),
                              ('velTex', mv), ('maskTex', mv)))
                self.g.u1i(L(p, 'taa2Inputs'), 0)
                self.g.u2f(L(p, 'invSize'), 1 / W, 1 / H)
                self.g.u2f(L(p, 'jitterPx'), jx, jy)
                self.g.u1i(L(p, 'taa2Tier'), tier)
                self.g.u1i(L(p, 'taa2Red'), red)
                self.g.u1i(L(p, 'taa2NoHist'), 1 if i == 0 else 0)
                self.g.u1i(L(p, 'wwDitherOn'), 0)
                self.um4(L(p, 'reproj'), 1, 0, reproj)
                gl.glDrawArrays(5, 0, 4)
                fp = self.p['taa2_rcas'] if tier >= 3 else self.p['game_taa_copy']
            self.targets(fin)
            self.bind(fp, (('srcTex', out),))
            if engine != 'game' and engine >= 3:
                self.g.u1f(L(fp, 'rcasSharp'), 2.0 ** -(0.5 if engine == 3 else 0.3))
            gl.glDrawArrays(5, 0, 4)
            yield self.read(fin)
        gl.glDeleteTextures(9, (GLuint * 9)(cur, dep, hist[0], hist[1], lock[0], lock[1], out, fin, mv))


Yf = lambda img: img @ np.array([0.2126, 0.7152, 0.0722])


def frames_for(nf, speed, with_objs, bar_of=None):
    fr = []
    for i in range(nf):
        k = i + 1
        n = (k & 7) + 1
        jx, jy = halton(2, n) - 0.5, halton(3, n) - 0.5
        rgb, d = frame(jx, jy, speed * i, with_objs, None if bar_of is None else bar_of(i), k)
        fr.append((rgb, d, speed))
    return fr


def frames_off(nf, speed, with_objs, bar_of=None):
    """TAA off: no jitter, no resolve (the plain frame)"""
    return [frame(0.0, 0.0, speed * i, with_objs, None if bar_of is None else bar_of(i))[0] for i in range(nf)]


def object_stats(outs, bgs, speed):
    """per object: v_f (visibility / area) over frames F0.., its contrast"""
    res = []
    for o in OBJS:
        vs = []
        for i in range(F0, len(outs)):
            cx, cy = int(o['x'] + speed * i), int(o['y'])
            ys, xs = slice(max(cy - 2, 0), cy + 3), slice(max(cx - 2, 0), min(cx + 3, W))
            bgc = Yf(bgs[i][cy, min(cx, W - 1)])
            c = Yf(o['col']) - bgc
            vs.append((Yf(outs[i][ys, xs]) - Yf(bgs[i][ys, xs])).sum() / c / (o['s'] ** 2) if xs.start < W else np.nan)
        vs = np.array(vs)
        c = abs(Yf(o['col']) - Yf(bgs[F0][int(o['y']), int(o['x'])]))
        res.append((o, vs, c))
    return res


def summary(stats):
    blink, flick, kept, perc = 0, [], [], 0
    worst = []
    for o, vs, c in stats:
        vs = vs[np.isfinite(vs)]
        if len(vs) < 4:
            continue
        kept.append(np.mean(vs))
        best = np.max(vs)
        if best * o['s'] ** 2 * c < T_VIS:
            continue
        perc += 1
        mu = max(np.mean(vs), 1e-6)
        flick.append(np.mean(np.abs(np.diff(vs))) / mu)
        if np.min(vs) < 0.5 * best:
            blink += 1
            worst.append((o['cls'], o['s'], round(float(np.min(vs)), 2), round(float(best), 2)))
    return dict(blink=blink, perc=perc, flicker=float(np.median(flick)) if flick else 0.0,
                kept=float(np.median(kept)) if kept else 0.0, worst=worst[:4])


def ghost(outs, bgs):
    """|dY| in the 8 px behind the bar's trailing edge against the same engine's run without the bar"""
    e = []
    for i in range(8, min(len(outs), 40)):
        x0 = int(40 + 2 * i)
        ys = slice(30, HOR - 20)
        e.append(np.abs(Yf(outs[i][ys, x0 - 8:x0]) - Yf(bgs[i][ys, x0 - 8:x0])).mean())
    return float(np.mean(e))


def main():
    nf = a.frames
    rig = Rig(a.shaders)
    print('taafar twin: shaders %s, %dx%d, MSAA %dx, %d objects (%d sizes), frames %d (steady from %d), red %d'
          % (a.shaders, W, H, a.msaa, len(OBJS), len(SIZES), nf, F0, a.red))
    sets = {}
    for mode, speed in (('still', 0.0), ('pan0.5', 0.5)):
        sets[mode] = (frames_for(nf, speed, True), frames_for(nf, speed, False), speed)
    bar_of = lambda i: 40 + 2 * i
    gfr = frames_for(40, 0.0, False, bar_of)
    gbg = frames_for(40, 0.0, False)
    results = {}
    for eng in a.engines.split(','):
        e = eng if eng in ('off', 'game') else int(eng)
        row = {}
        for mode, (fo, fb, speed) in sets.items():
            if e == 'off':
                outs = frames_off(nf, speed, True)
                bgs = frames_off(nf, speed, False)
            else:
                outs = list(rig.run(e, fo, a.red))
                bgs = list(rig.run(e, fb, a.red))
            row[mode] = summary(object_stats(outs, bgs, speed))
            if a.png and mode == 'still' and e != 'off':
                from PIL import Image
                k6 = lambda im: np.kron((np.clip(im[::-1][24:88, 0:160], 0, 1) * 255).astype(np.uint8), np.ones((4, 4, 1), np.uint8))
                strip = np.concatenate([k6(outs[i]) for i in range(F0, F0 + 4)], 0)
                os.makedirs(a.png, exist_ok=True)
                Image.fromarray(strip).save(os.path.join(a.png, 'still_%s_red%d.png' % (eng, a.red)))
        if e == 'off':
            row['ghost'] = ghost(frames_off(40, 0.0, False, bar_of), frames_off(40, 0.0, False))
        else:
            row['ghost'] = ghost(list(rig.run(e, gfr, a.red)), list(rig.run(e, gbg, a.red)))
        results[eng] = row
        s, m = row['still'], row['pan0.5']
        print('engine %-5s still: blink %3d of %3d perceptible, flicker %.3f, kept %.2f | pan0.5: blink %3d of %3d, flicker %.3f, kept %.2f | ghost %.4f'
              % (eng, s['blink'], s['perc'], s['flicker'], s['kept'], m['blink'], m['perc'], m['flicker'], m['kept'], row['ghost']))
        if s['worst']:
            print('             still worst (class, size px, min v, max v): %s' % s['worst'])
    if a.verdict:
        r = results[a.verdict]
        gb = a.ghost_bar if a.ghost_bar >= 0 else r['ghost']
        ok = (r['still']['blink'] == 0 and r['still']['flicker'] <= 0.15 and r['pan0.5']['flicker'] <= 0.25
              and r['ghost'] <= gb + 0.002)
        print('VERDICT %s engine %s: still blink %d (0), still flicker %.3f (<= 0.15), pan flicker %.3f (<= 0.25), ghost %.4f (<= %.4f + 0.002)'
              % ('PASS' if ok else 'FAIL', a.verdict, r['still']['blink'], r['still']['flicker'], r['pan0.5']['flicker'], r['ghost'], gb))


main()
