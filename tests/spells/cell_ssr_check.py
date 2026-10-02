"""The cell view's screen-space reflections, rebuilt independently (lane SSR1; tests/spells/cell_ssr.sh).

Reads ONE view's WW_CELL_SSR_DUMP (the viewer's own depth + normals, its scene color with the flag, and its
three outputs) and re-does the game's chain in numpy from the lane's notes, not from the viewer's shader:
the ray (view ray mirrored about the normal, world z doubled first; kept when it points more than 0.2 into
the view; run back to the near plane), the 32-step march over a min-of-2x2 depth pyramid built HERE (4 levels;
a ray 50 units or more behind the finest level is refused), the confidence (screen edge, travelled distance,
25 x depth gained / (far - near)) squared, and the 5-tap blur across and down. near = 15; far = the cell's clip
distance, read HERE from the plugin (XCLL offset 32, or the lighting template's when Inherits has 0x80).

Stages (each prints "<stage> PASS|FAIL|SKIP ..."):
  F  the viewer's far plane is the plugin's clip distance
  M  the march: the viewer's raw result vs the rebuild, where the rebuild expects a reflection
  B  the blur: the viewer's final vs the rebuild's final
  P  the picture: probe 61 (the reflection as each draw read it) vs the rebuild's final, upsampled
  L  the picture against the one shot with the reflection off: it changes where the rebuild has a reflection,
     gets brighter there on average, and changes nowhere else
  Z  nothing expected: the probe is black and the picture equals the one shot with the reflection off
Bars: agree >= 99% (M, B) or 95% (P, an 8-bit picture) of the pixels where the value shows, within 0.002 + 2%,
AND viewer total / expected total within 5%.
A stage with under 300 pixels showing the value says SKIP. "ssr PASS" needs F, and M + P passing with B and L
not failing or, in a view named zero (or where M, B, P all skip), Z passing.

USAGE  python cell_ssr_check.py <esm> <cell> <run dir> [zero]
       <run dir> holds ssr.bin, ssr.bin.txt, p61.png, and (for L and Z) on.png + off.png
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cell_fog_check                                                       # noqa: E402  (the plugin walk only)

NEAR, FAR_MAX = 15.0, 353840.0
ANGLE_GATE, NORMAL_Z, CONF_SCALE = 0.2, 2.0, 1.0
GAP, FADE = 50.0, 25.0
DITHER = np.array([0, .5, .125, .625, .75, .22, .875, .375, .1875, .6875, .0625, .5625, .9375, .4375, .8125, .3125],
                  np.float32)
TAP_OFF = (-3.294215, -1.407333, 0.0, 1.407333, 3.294215)
TAP_W = (0.093913, 0.304005, 0.204164, 0.304005, 0.093913)
MIN_PIXELS = 300
F32 = np.float32


def clip_distance(esm, cell):
    """The game's interior far plane: min(353840, clip distance when > 0)."""
    x, t = cell_fog_check.interiors(esm, [cell])[cell]
    inh = struct.unpack_from('<I', x, 88)[0] if len(x) >= 92 else 0
    src = t if (t and (not x or inh & 0x80)) else x
    clip = struct.unpack_from('<f', src, 32)[0] if len(src) >= 36 else 0.0
    return min(FAR_MAX, clip if clip > 0 else FAR_MAX)


def load(run):
    b = open(os.path.join(run, 'ssr.bin'), 'rb').read()
    W, H, hw, hh = struct.unpack_from('<4i', b, 0)
    o = 16
    full = W * H * 4
    half = hw * hh * 4
    gb = np.frombuffer(b, '<f4', full, o).reshape(H, W, 4)[::-1]            # top row first from here on
    o += full * 4
    sc = np.frombuffer(b, '<f4', full, o).reshape(H, W, 4)[::-1]
    o += full * 4
    ray = np.frombuffer(b, '<f4', half, o).reshape(hh, hw, 4)
    o += half * 4
    raw = np.frombuffer(b, '<f4', half, o).reshape(hh, hw, 4)
    o += half * 4
    fin = np.frombuffer(b, '<f4', half, o).reshape(hh, hw, 4)
    txt = open(os.path.join(run, 'ssr.bin.txt')).read().split()
    num = {k: float(txt[txt.index(k) + 1]) for k in ('far', 'p00', 'p11')}
    i = txt.index('rot')
    rot = np.array([float(v) for v in txt[i + 1:i + 10]]).reshape(3, 3).T    # the file is column by column
    return dict(W=W, H=H, hw=hw, hh=hh, gb=gb, sc=sc, ray=ray, raw=raw, fin=fin, rot=rot, **num)


def pyramid(z):
    """Levels 1..4 of the min-of-2x2 depth (floor sizes)."""
    out = [z]
    for _ in range(4):
        p = out[-1]
        h, w = max(p.shape[0] // 2, 1), max(p.shape[1] // 2, 1)
        q = p[:h * 2, :w * 2] if p.shape[0] >= 2 and p.shape[1] >= 2 else np.repeat(np.repeat(p, 2, 0), 2, 1)[:h * 2, :w * 2]
        out.append(np.minimum(np.minimum(q[0::2, 0::2], q[0::2, 1::2]), np.minimum(q[1::2, 0::2], q[1::2, 1::2])))
    return out


def rays(d):
    """(uv0, start depth) per half-size pixel; zero where no ray starts."""
    W, H, hw, hh = d['W'], d['H'], d['hw'], d['hh']
    py, px = np.mgrid[0:hh, 0:hw]
    tx = np.minimum(((2 * px + 1) * W) // (2 * hw), W - 1)                   # the full-size texel under the centre
    ty = np.minimum(((2 * py + 1) * H) // (2 * hh), H - 1)
    g = d['gb'][ty, tx].astype(np.float64)
    flag = d['sc'][ty, tx, 3].astype(np.float64)
    u = (px + 0.5) / hw
    v = (py + 0.5) / hh
    z = g[..., 3]
    P = np.stack([(2 * u - 1) * z / d['p00'], (1 - 2 * v) * z / d['p11'], -z], -1)
    V = -P / np.maximum(np.linalg.norm(P, axis=-1, keepdims=True), 1e-20)
    N = g[..., :3]
    facing = (N * V).sum(-1) >= 0
    Nw = N @ d['rot'].T
    Nw[..., 2] *= NORMAL_Z
    Nw /= np.maximum(np.linalg.norm(Nw, axis=-1, keepdims=True), 1e-20)
    Nv = Nw @ d['rot']
    I = -V
    R = I - 2 * (Nv * I).sum(-1, keepdims=True) * Nv
    keep = (flag * CONF_SCALE - 0.01 >= 0) & facing & (-R[..., 2] > ANGLE_GATE)
    Q = P + 1000.0 * R
    zq = -Q[..., 2]
    with np.errstate(all='ignore'):
        uq = 0.5 + 0.5 * d['p00'] * Q[..., 0] / zq
        vq = 0.5 - 0.5 * d['p11'] * Q[..., 1] / zq
        s = (1 / NEAR - 1 / z) / (1 / zq - 1 / z)
        u0 = u + (uq - u) * s
        v0 = v + (vq - v) * s
    out = np.zeros((hh, hw, 3))
    out[..., 0] = np.where(keep, u0, 0)
    out[..., 1] = np.where(keep, v0, 0)
    out[..., 2] = np.where(keep, z, 0)
    return out


def bilinear(img, x, y):
    """img[y, x] at continuous texel coordinates (texel centres at integers), clamped to the edge."""
    h, w = img.shape[:2]
    x = np.clip(x, 0, w - 1)
    y = np.clip(y, 0, h - 1)
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    x1 = np.minimum(x0 + 1, w - 1)
    y1 = np.minimum(y0 + 1, h - 1)
    fx = (x - x0)[..., None]
    fy = (y - y0)[..., None]
    return (img[y0, x0] * (1 - fx) + img[y0, x1] * fx) * (1 - fy) + (img[y1, x0] * (1 - fx) + img[y1, x1] * fx) * fy


def march(d, ray, far, gap=True, fade=True):
    """The 32-step march, float32 like the viewer's; returns (hh, hw, 4): rgb, confidence squared."""
    hw, hh = d['hw'], d['hh']
    mips = pyramid(d['gb'][..., 3].astype(F32))
    n = hw * hh
    py, px = np.mgrid[0:hh, 0:hw]
    px = px.reshape(n)
    py = py.reshape(n)
    uv = np.stack([(px + 0.5) / hw, (py + 0.5) / hh], -1).astype(F32)
    z0 = ray[..., 2].reshape(n).astype(F32)
    started = z0 > NEAR
    z0s = np.where(started, z0, F32(1))
    dj = DITHER[(py & 3) + 4 * (px & 3)]
    A = np.concatenate([ray[..., :2].reshape(n, 2).astype(F32), (F32(1 / NEAR) + (dj - F32(0.5)) * F32(0.004))[:, None]], 1)
    D = np.concatenate([uv, (F32(1) / z0s)[:, None]], 1) - A
    S = uv + D[:, :2] * (dj * F32(0.002))[:, None]
    G = np.tile(np.floor(np.array([hw, hh], F32) / F32(8)), (n, 1))
    cell = np.floor(G * S)
    level = np.full(n, 4)
    s01 = (D[:, :2] >= 0).astype(F32)
    sgn = 2 * s01 - 1

    def cross(cell, G):
        with np.errstate(all='ignore'):
            t = ((cell + s01) / G - A[:, :2]) / D[:, :2]
            xfirst = t[:, 1] >= t[:, 0]
            stepv = sgn * np.stack([xfirst, ~xfirst], -1).astype(F32)
            p = A + np.fmin(t[:, 0], t[:, 1])[:, None] * D
        return cell + stepv, p, stepv

    def out_of(p):
        with np.errstate(all='ignore'):
            return (p[:, 0] <= 0) | (p[:, 0] >= 1) | (p[:, 1] <= 0) | (p[:, 1] >= 1) | (F32(1) / p[:, 2] >= far)

    def depth_at(level, cell):
        dd = np.zeros(n, F32)
        with np.errstate(all='ignore'):
            t = np.nan_to_num(np.trunc(cell), nan=-1, posinf=-1, neginf=-1).astype(np.int64)
        for L in range(1, 5):
            m = mips[L]
            pick = (level == L) & (t[:, 0] >= 0) & (t[:, 1] >= 0) & (t[:, 0] < m.shape[1]) & (t[:, 1] < m.shape[0])
            dd[pick] = m[t[pick, 1], t[pick, 0]]
        return dd

    cell, p, stepv = cross(cell, G)
    with np.errstate(all='ignore'):
        zfirst = F32(1) / p[:, 2]
    alive = started.copy()
    refused = np.zeros(n, bool)
    count = np.zeros(n, int)
    dlast = np.zeros(n, F32)
    for step in range(1, 33):
        alive &= (level >= 1) & ~out_of(p)
        if not alive.any():
            break
        dd = depth_at(level, cell)
        with np.errstate(all='ignore'):
            rz = F32(1) / p[:, 2]
            isl1 = (level == 1) & gap
            ref = refused | (isl1 & (rz - dd >= GAP))
            down = alive & (dd < rz) & ~ref
            other = alive & ~down
            h = (p[:, :2] - (cell + F32(0.5)) / G >= 0).astype(F32)
            cell_c, p_c, stepv_c = cross(cell, G)
            rz2 = F32(1) / p_c[:, 2]
            ref2 = ref | (isl1 & (rz2 - dd >= GAP))
            hit = other & ~ref2 & (dd < rz2)
            climb = other & ~ref2 & ~(dd < rz2) & (level != 4)
            p_h = A + ((F32(1) / dd - A[:, 2]) / D[:, 2])[:, None] * D
            cell_h = np.floor(2 * G * p_h[:, :2])
            cell_u = cell_c / 2 + np.where(stepv_c >= 0, F32(0), F32(-0.5))
        plain = other & ~hit & ~climb
        new_cell = np.where(down[:, None], 2 * cell + h, np.where(hit[:, None], cell_h, np.where(climb[:, None], cell_u,
                            np.where(plain[:, None], cell_c, cell))))
        new_G = np.where((down | hit)[:, None], 2 * G, np.where(climb[:, None], np.floor(G / 2), G))
        new_p = np.where(hit[:, None], p_h, np.where(other[:, None], p_c, p))
        level = np.where(down | hit, level - 1, np.where(climb, level + 1, level))
        stepv = np.where(other[:, None], stepv_c, stepv)
        refused = np.where(other, ref2, refused)
        cell, G, p = new_cell.astype(F32), new_G.astype(F32), new_p.astype(F32)
        dlast = np.where(alive, dd, dlast)
        count = np.where(alive, step, count)
    miss = ~started | out_of(p) | (count == 32) | refused
    with np.errstate(all='ignore'):
        c = 1 - np.minimum(2 * np.linalg.norm(p[:, :2] - 0.5, axis=1), 1) ** 2
        c = c * np.maximum(1 - 2 * np.linalg.norm(S - p[:, :2], axis=1), 0)
        c = c * np.clip(1 - FADE * (dlast - zfirst) / (far - NEAR), 0, 1)
    if not fade:
        c = np.ones(n)
    c = np.where(miss, 0, np.nan_to_num(c))
    pu = np.nan_to_num(p[:, 0].astype(np.float64))
    pv = np.nan_to_num(p[:, 1].astype(np.float64))
    rgb = np.clip(bilinear(d['sc'][..., :3].astype(np.float64), pu * d['W'] - 0.5, pv * d['H'] - 0.5), 0, 1)
    out = np.zeros((n, 4))
    out[:, :3] = np.where(miss[:, None], 0, rgb)
    out[:, 3] = c * c
    return out.reshape(hh, hw, 4)


def blur(src, axis):
    hh, hw = src.shape[:2]
    y, x = np.mgrid[0:hh, 0:hw].astype(np.float64)
    taps = [bilinear(src, x + (o if axis == 0 else 0), y + (o if axis == 1 else 0)) for o in TAP_OFF]
    a = sum(w * t[..., 3] for w, t in zip(TAP_W, taps))
    missing = [t[..., 3] < 0.01 for t in taps]
    lost = sum(w * m for w, m in zip(TAP_W, missing))
    present = 5 - sum(m.astype(int) for m in missing)
    e = np.where(present > 0, lost / np.maximum(present, 1), 0)
    rgb = sum(np.where(m[..., None], 0, (w + e)[..., None] * t[..., :3]) for w, m, t in zip(TAP_W, missing, taps))
    return np.concatenate([rgb, a[..., None]], -1)


def light(x):
    """What a draw adds from a reflection texel: the mean of rgb x min(confidence, 1)."""
    return x[..., :3].mean(-1) * np.clip(x[..., 3], 0, 1)


def judge(name, got, exp, tol_abs, shows_at, spread=None, bar=0.99):
    """agree where the expectation OR the viewer shows, and the totals' ratio; returns (verdict, line)."""
    # both sides: a march that skips the 50-unit refusal adds hits where the rebuild has none, and a mask taken
    # from the expectation alone never looks there (the live nogap red passed at 100.0% before this)
    shows = (exp > shows_at) | (got > shows_at)
    npx = int(shows.sum())
    if npx < MIN_PIXELS:
        return 'SKIP', '%s SKIP (%d pixels show the value, under %d)' % (name, npx, MIN_PIXELS)
    # measured on two walkway views: the viewer is within 0.0012 of the rebuild on 99.9% of the march's and the
    # blur's pixels (the same float32 steps), so 0.002 + 2% is slack. The bar is 99%, not 95: a march without the
    # 50-unit refusal still agrees on 93..96% of these views' pixels, and has to fail
    tol = tol_abs + 0.02 * exp + (spread if spread is not None else 0)
    agree = (np.abs(got - exp) <= tol)[shows].mean()
    ratio = got[shows].sum() / max(exp[shows].sum(), 1e-9)
    ok = agree >= bar and 0.95 <= ratio <= 1.05
    return ('PASS' if ok else 'FAIL'), '%s %s agree %.1f%% of %d pixels (bar %d), viewer / expected total %.3f (bar 0.95..1.05)' % (
        name, 'PASS' if ok else 'FAIL', 100 * agree, npx, round(100 * bar), ratio)


def expected(d, far, gap=True, fade=True):
    ray = rays(d)
    raw = march(d, ray, far, gap, fade)
    fin = blur(blur(raw, 0), 1)
    return ray, raw, fin


def upsample(d, fin):
    """The reflection a full-size pixel's draw reads: bilinear, only where the opaque surface carries the flag."""
    H, W = d['H'], d['W']
    r, c = np.mgrid[0:H, 0:W].astype(np.float64)
    up = bilinear(fin, (c + 0.5) / W * d['hw'] - 0.5, (r + 0.5) / H * d['hh'] - 0.5)
    flagged = d['sc'][..., 3] > 0.5
    return np.where(flagged[..., None], up, 0), flagged


def main(esm, cell, run, zero=False, exp=None):
    lines = []
    d = load(run)
    far = clip_distance(esm, cell)
    okF = abs(far - d['far']) <= 0.01
    lines.append('F %s the far plane: the plugin says %.2f, the viewer used %.2f' % ('PASS' if okF else 'FAIL', far, d['far']))
    ray, raw, fin = exp if exp is not None else expected(d, far)
    started = int((ray[..., 2] > NEAR).sum())
    hitn = int((raw[..., 3] > 0).sum())
    lines.append('  rebuild: %d of %d half pixels start a ray, %d hit, mean confidence %.4f' % (
        started, d['hw'] * d['hh'], hitn, float(np.clip(fin[..., 3], 0, 1).mean())))
    # the blur stage reads the VIEWER's march, so a march difference does not count twice
    fin_of_viewer = blur(blur(d['raw'].astype(np.float64), 0), 1)
    vM, lM = judge('M', light(d['raw'].astype(np.float64)), light(raw), 0.002, 0.01)
    vB, lB = judge('B', light(d['fin'].astype(np.float64)), light(fin_of_viewer), 0.002, 0.01)
    lines += [lM, lB]
    # the picture: probe 61 = sqrt(c), sqrt(green x c), sqrt(mean x c)
    p = np.asarray(Image.open(os.path.join(run, 'p61.png')).convert('RGB'), np.float64) / 255.0
    up, flagged = upsample(d, fin)
    exp_pic = up[..., :3].mean(-1) * np.clip(up[..., 3], 0, 1)
    got_pic = p[..., 2] ** 2
    step = 2 * np.sqrt(np.maximum(exp_pic, 0)) / 255 + 1.0 / 255 ** 2       # one 8-bit step of the square root
    vP, lP = ('SKIP', 'P SKIP (the picture is not the dump\'s size)') if p.shape[:2] != (d['H'], d['W']) else \
        judge('P', got_pic, exp_pic, 0.002, 0.01, step, 0.95)
    lines.append(lP)
    verdicts = [vM, vB, vP]
    # the picture with the reflections against the one shot without them (WW_CELL_SSR_RED=off)
    change = None
    on, off = os.path.join(run, 'on.png'), os.path.join(run, 'off.png')
    if os.path.exists(on) and os.path.exists(off):
        a = np.asarray(Image.open(on).convert('RGB'), int)
        b = np.asarray(Image.open(off).convert('RGB'), int)
        if a.shape == b.shape and a.shape[:2] == (d['H'], d['W']):
            change = np.abs(a - b).max(-1)
    if all(v == 'SKIP' for v in verdicts) or zero:
        got_c = float((p[..., 0] ** 2).sum()) if p.shape[:2] == (d['H'], d['W']) else -1.0
        same = None if change is None else int(change.max()) == 0
        exp_c = float(np.clip(up[..., 3], 0, 1).sum())
        nflag = int(flagged.sum())
        okZ = exp_c < 1.0 and got_c == 0.0 and same is True and nflag >= MIN_PIXELS
        lines.append('Z %s nothing expected (%d pixels carry the flag, the rebuild starts %d rays, its confidence total '
                     '%.3f): the probe\'s confidence total %.3f, the picture %s the one without reflections' % (
                         'PASS' if okZ else 'FAIL', nflag, started, exp_c, got_c,
                         'EQUALS' if same else 'missing' if same is None else 'DIFFERS FROM'))
        ok = okF and okZ and not any(v == 'FAIL' for v in verdicts)
    else:
        # L: the reflections change the picture where the rebuild has them and nowhere else (3 pixels of slack
        # for the bilinear read and the antialiased edges)
        vL = 'SKIP'
        if change is None:
            lines.append('L SKIP (no on.png + off.png pair of the dump\'s size)')
        else:
            near = np.clip(up[..., 3], 0, 1) > 0
            for _ in range(3):
                g = near.copy()
                g[1:] |= near[:-1]
                g[:-1] |= near[1:]
                g[:, 1:] |= near[:, :-1]
                g[:, :-1] |= near[:, 1:]
                near = g
            stray = int((change[~near] > 0).sum())
            shown = exp_pic > 0.01
            moved = int((change[shown] > 0).sum())
            gain = float((a - b).mean(-1)[shown].mean()) if shown.any() else 0.0
            okL = stray == 0 and moved >= MIN_PIXELS and gain > 0
            vL = 'PASS' if okL else 'FAIL'
            lines.append('L %s the picture changes on %d of the %d pixels the rebuild lights (bar %d), by %+.2f / 255 on '
                         'average (bar > 0), and on %d pixels away from every reflection (bar 0)' % (
                             vL, moved, int(shown.sum()), MIN_PIXELS, gain, stray))
        ok = okF and not any(v == 'FAIL' for v in verdicts + [vL]) and vM == 'PASS' and vP == 'PASS'
    lines.append('ssr %s' % ('PASS' if ok else 'FAIL'))
    return '\n'.join(lines)


if __name__ == '__main__':
    print(main(sys.argv[1], sys.argv[2], sys.argv[3], len(sys.argv) > 4 and sys.argv[4] == 'zero'))
