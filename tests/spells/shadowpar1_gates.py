"""lane SHADOWPAR1: the judge of tests/spells/shadowpar1_gates.sh (pictures + echoes, never the code under test).

usage: python shadowpar1_gates.py --out DIR [--red NAME]
One verdict line per gate, INFO lines for what is measured but not gated, and a last line ALL PASS / FAIL.
With --red no<item> the item's gate must FAIL (the item back on the old path).
Tolerances are stated beside each gate before it measures.
"""
import argparse
import math
import os
import re

import numpy as np
from PIL import Image

CUBE_H = 512.0      # QryCube01: a 512-unit cube on the origin, the Lookdev ground at z = -256
SHOT_W = 1600


def img(path):
    return np.asarray(Image.open(path).convert('RGB'), dtype=np.float64) / 255.0


def grey(a, tol=0.02):
    """the debug views draw their factor as grey; the sky (blue) is not the factor"""
    return (np.abs(a[:, :, 0] - a[:, :, 1]) < tol) & (np.abs(a[:, :, 1] - a[:, :, 2]) < tol)


def cam_echo(text):
    """(eye x, y, z, tanX, tanY) from the csm echo's cam= field, or None"""
    m = re.search(r'cam=([-\d.e]+),([-\d.e]+),([-\d.e]+)\|[^|]*\|([-\d.e]+),([-\d.e]+)\|', text)
    return tuple(float(v) for v in m.groups()) if m else None


def lum(a):
    return 0.2126 * a[:, :, 0] + 0.7152 * a[:, :, 1] + 0.0722 * a[:, :, 2]


def echo(path):
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            return f.read().strip()
    except OSError:
        return ''


def num(pat, text, default=None):
    m = re.search(pat, text)
    return float(m.group(1)) if m else default


def edge_width(prof):
    """10-90 % width (samples) of the transitions at both ends of the dark run in a 0..1 profile; None if no run."""
    dark = np.where(prof < 0.5)[0]
    if dark.size == 0:
        return None
    widths = []
    for lo_side in (True, False):
        i = dark[0] if lo_side else dark[-1]
        step = -1 if lo_side else 1
        # walk out of the core to 0.9 and into it to 0.1
        a = i
        while 0 <= a + step < prof.size and prof[a] < 0.9:
            a += step
        b = i
        while 0 <= b - step < prof.size and prof[b] > 0.1:
            b -= step
        widths.append(abs(a - b))
    return 0.5 * (widths[0] + widths[1])


def end_width(prof):
    """10-90 % width (samples) of the dark run's end that does not touch the picture's border; None if none"""
    dark = np.where(prof < 0.5)[0]
    if dark.size == 0:
        return None
    for i, step in ((dark[0], -1), (dark[-1], 1)):
        if 3 <= i <= prof.size - 4:
            a = i
            while 0 <= a + step < prof.size and prof[a] < 0.9:
                a += step
            b = i
            while 0 <= b - step < prof.size and prof[b] > 0.1:
                b -= step
            if 0 < a < prof.size - 1:
                return abs(a - b)
    return None


def side_edges(a, upp, cx, xs, d, half):
    """a top-down ortho terrain view's shadow side edges at the world columns xs: the mean distance (world units) of
    the measured edges from the cube's analytic sides (a line through the origin along d, +-half across it).
    Returns (mean error, columns, mean sign of the run's side) or (None, reason)."""
    H, W = a.shape
    errs, signs = [], []
    k = d[1] / d[0]
    off = half / abs(d[0])          # the sides' vertical offset from the centre line
    for x in xs:
        c = int(round(W / 2 + (x - cx) / upp))
        if not 0 <= c < W:
            continue
        col = a[:, c]
        dark = col < 0.5
        if not dark.any():
            continue
        best, run, start = (0, 0, 0), 0, 0       # the longest dark run in the column
        for r in range(H + 1):
            if r < H and dark[r]:
                if run == 0:
                    start = r
                run += 1
            else:
                if run > best[0]:
                    best = (run, start, r - 1)
                run = 0
        _, r0, r1 = best
        if r0 == 0 or r1 == H - 1:
            continue

        def cross(rl, rd):                       # sub-pixel 0.5 crossing; row centres at y = (H / 2 - r - 0.5) upp
            vl, vd = col[rl], col[rd]
            f = (vl - 0.5) / max(vl - vd, 1e-6)
            return (H / 2.0 - (rl + f * (rd - rl)) - 0.5) * upp
        y_top, y_bot = cross(r0 - 1, r0), cross(r1 + 1, r1)
        yc = 0.5 * (y_top + y_bot)
        s = 1.0 if yc * (x * k) >= 0 else -1.0   # the picture's y may be flipped: take the side the run is on
        yl = s * x * k
        if abs(yc - yl) > 100.0:
            return None, 'column x=%.0f: the shadow centre %.0f is not on the sun line %.0f (camera/sun convention)' % (x, yc, yl)
        signs.append(s)
        errs += [abs(y_top - (yl + off)) * abs(d[0]), abs(y_bot - (yl - off)) * abs(d[0])]
    if len(errs) < 40:
        return None, 'only %d edge samples' % len(errs)
    return float(np.mean(errs)), len(errs) // 2, float(np.mean(signs))


class Judge:
    def __init__(self, out, red):
        self.out = out
        self.red = red
        self.ok = True
        self.ran = 0

    def verdict(self, name, ok, detail):
        self.ran += 1
        self.ok &= bool(ok)
        print('%s %s %s' % ('PASS' if ok else 'FAIL', name, detail), flush=True)

    def info(self, name, detail):
        print('INFO %s %s' % (name, detail), flush=True)

    def f(self, name):
        return os.path.join(self.out, name)

    def have(self, *names):
        return all(os.path.isfile(self.f(n)) for n in names)

    def uss(self, tag):
        return echo(self.f(tag + '.uss.txt'))

    # -- off: the item set off = the rung byte for byte (every TERRSHADOW1 producer on), and the echo says par=0
    def off(self):
        if not self.have('off_cube_old.png'):
            return
        ref = open(self.f('off_cube_old.png'), 'rb').read()
        for arm in ('new', 'pinned'):
            p = self.f('off_cube_%s.png' % arm)
            same = os.path.isfile(p) and open(p, 'rb').read() == ref
            e = self.uss('off_cube_%s' % arm)
            quiet = ' par=0 [none]' in e
            self.verdict('off_cube_%s' % arm, same and quiet, 'byte-identical=%s echo-par0=%s' % (same, quiet))

    # -- dist: the cascades reach 24000 under the item (tolerance: exact)
    def dist(self):
        if not self.have('dist_on.png'):
            return
        d = num(r'\(D=([\d.]+)', self.uss('dist_on'))
        self.verdict('dist', d == 24000.0, 'D=%s want 24000' % d)

    # -- cradius: the cascades' penumbra in world units (High vs Ultra texels differ): the width ratio within 1 +- 0.3.
    # FIXGATES1: a perspective top-down eye (the cascades refuse ortho); the ground's units per pixel from the echoed
    # camera (2 tanX h / W); the profile runs along x through the shadow's END (x = -1143), the edge the sun grazes
    def cradius(self):
        r = {}
        for arm in ('base', 'on'):
            w = {}
            for t in (3, 4):
                tag = 'crad_%d_%s' % (t, arm)
                p = self.f(tag + '.png')
                if not os.path.isfile(p):
                    return
                ce = echo(self.f(tag + '.csm.txt'))
                cam = cam_echo(ce)
                if not ce.startswith('csm=on') or cam is None:
                    self.verdict('cradius', False, '%s: the cascades did not draw (%s)' % (tag, ce[:40]))
                    return
                a = lum(img(p))
                upp = 2.0 * cam[3] * (cam[2] + 256.0) / a.shape[1]
                mid = a.shape[0] // 2
                prof = a[mid - 20:mid + 21, :].mean(axis=0)
                prof = (prof - prof.min()) / max(prof.max() - prof.min(), 1e-6)
                ew = end_width(prof)
                w[t] = None if ew is None else ew * upp
            if None in w.values():
                self.verdict('cradius', False, '%s: no shadow end found (%s)' % (arm, w))
                return
            r[arm] = (w[3], w[4], w[3] / max(w[4], 1e-6))
        self.info('cradius_base', 'High=%.1f u Ultra=%.1f u ratio=%.2f' % r['base'])
        ok = abs(r['on'][2] - 1.0) <= 0.3
        self.verdict('cradius', ok, 'High=%.1f u Ultra=%.1f u ratio=%.2f (tolerance 1 +- 0.3)' % r['on'])

    def terr_run(self, tag, hw, cx):
        """the terrain view's shadow on the ground walking -x from the cube's west face; returns (length, row, upp)."""
        a = img(self.f(tag + '.png'))[:, :, 0]
        upp = 2.0 * hw / a.shape[1]
        mid = a.shape[0] // 2
        row = a[mid - 2:mid + 3, :].mean(axis=0)

        def px(x):
            return int(round(a.shape[1] / 2 + (x - cx) / upp))
        start = max(px(-256.0) - 2, 0)
        end = start
        while end > 0 and row[end] < 0.5:
            end -= 1
        return (start - end + 2) * upp if row[start] < 0.5 else 0.0, a, upp, px

    # -- near: the near tier (64 u texels) puts the shadow's side edges closer to the cube's analytic sides than the far
    # map (160 u) alone: mean error on <= 0.6 x base. FIXGATES1: sun from 110 (the edges cross the texel rows, so the
    # error is the texel's, not the grid's luck); BATCH1's 10-90 width was 2 u in both arms (hard steps, no width)
    def near(self):
        if not self.have('near_base.png', 'near_on.png'):
            return
        az = math.radians(110.0)
        d = (-math.sin(az), -math.cos(az))                  # the shadow runs away from the sun
        half = 256.0 * (abs(d[0]) + abs(d[1]))              # the cube's half-extent across the sun line
        e = {}
        for arm in ('base', 'on'):
            a = img(self.f('near_%s.png' % arm))[:, :, 0]
            upp = 2.0 * 2000.0 / a.shape[1]
            res = side_edges(a, upp, -1000.0, np.arange(-1800.0, -500.0, upp), d, half)
            if res[0] is None:
                self.verdict('near', False, '%s: %s' % (arm, res[1]))
                return
            e[arm] = res
        self.info('near_base', 'side error %.1f u over %d columns' % e['base'][:2])
        ok = e['base'][0] > 2.0 * 2.5 and e['on'][0] <= 0.6 * e['base'][0]
        self.verdict('near', ok, 'side error on=%.1f u base=%.1f u (tolerance on <= 0.6 base; base > 5 u, the gate sees)' % (
            e['on'][0], e['base'][0]))

    # -- tslope: FO4CS's bias 12 (1 + slope) / sin(elev) on flat ground at 10 degrees: the shadow is
    # 512 / tan 10 - (12 / sin 10) / tan 10 = 2512 u long (tolerance +- 120: two near texels and the bilinear edge)
    def tslope(self):
        if not self.have('tslope_base.png', 'tslope_on.png'):
            return
        e = math.radians(10.0)
        want = CUBE_H / math.tan(e) - (12.0 / math.sin(e)) / math.tan(e)
        lb = self.terr_run('tslope_base', 2000.0, -1400.0)[0]
        lo = self.terr_run('tslope_on', 2000.0, -1400.0)[0]
        self.info('tslope_base', 'length=%.0f u (the old bias)' % lb)
        self.verdict('tslope', abs(lo - want) <= 120.0, 'length=%.0f u want %.0f +- 120' % (lo, want))

    # -- lowsun: no hard cutoff; FO4CS's smoothstep over 1..4 degrees: at 2.5 the core reads 0.5 +- 0.08, at 0.5 all lit
    def lowsun(self):
        for e, want in (('2.5', 0.5), ('0.5', 1.0)):
            if not self.have('lowsun_%s_on.png' % e):
                continue
            a = img(self.f('lowsun_%s_on.png' % e))[:, :, 0]
            mid = a.shape[0] // 2
            upp = 2.0 * 12000.0 / a.shape[1]
            # the core: x from -1000 to -6000 behind the cube, the middle rows
            x0 = int(round(a.shape[1] / 2 + (-6000.0 + 11000.0) / upp))
            x1 = int(round(a.shape[1] / 2 + (-1000.0 + 11000.0) / upp))
            core = float(np.median(a[mid - 3:mid + 4, x0:x1]))
            if e == '0.5':
                ok = core >= 0.95 and float(a.min()) >= 0.9
                self.verdict('lowsun_0.5', ok, 'core=%.3f min=%.3f (want all lit: core >= 0.95, min >= 0.9)' % (core, a.min()))
            else:
                ok = abs(core - want) <= 0.08
                self.verdict('lowsun_2.5', ok, 'core=%.3f want 0.5 +- 0.08' % core)
            if self.have('lowsun_%s_base.png' % e):
                b = img(self.f('lowsun_%s_base.png' % e))[:, :, 0]
                self.info('lowsun_%s_base' % e, 'core=%.3f (the old 0.05-degree cutoff)' % float(np.median(b[mid - 3:mid + 4, x0:x1])))

    # -- moon: FO4CS's 0.15 phase^3 (game day 0 full = 0.150, day 12 new = 0.000); tolerance: the echo's 3 decimals
    def moon(self):
        for d, want in (('0', 0.150), ('12', 0.0)):
            if not self.have('moon_%s_on.png' % d):
                continue
            s = num(r'strength=([\d.]+)', self.uss('moon_%s_on' % d))
            self.verdict('moon_day%s' % d, s is not None and abs(s - want) < 0.0015, 'strength=%s want %.3f' % (s, want))
            sb = num(r'strength=([\d.]+)', self.uss('moon_%s_base' % d))
            self.info('moon_day%s_base' % d, 'strength=%s' % sb)

    # -- cslope: the cascades' slope clamp on (echo of the fit)
    def cslope(self):
        if not self.have('cslope_on.png'):
            return
        c = num(r'slopeclamp=(\d+)', echo(self.f('cslope_on.csm.txt')))
        self.verdict('cslope', c == 1.0, 'slopeclamp=%s want 1' % c)

    def pair(self, a_tag, b_tag):
        if not self.have(a_tag + '.png', b_tag + '.png'):
            return None
        a, b = img(self.f(a_tag + '.png')), img(self.f(b_tag + '.png'))
        if a.shape != b.shape:
            return None
        return a, b

    # -- objlod: far-LOD objects in the heightfield: echo count in (0, 1500], and the terrain view darkens
    # (dark fraction up by >= 0.1 % of the picture)
    def objlod(self):
        p = self.pair('c_v2_none', 'c_v2_objlod')
        if p is None:
            return
        n = num(r'objlod=(\d+)', self.uss('c_v2_objlod'), 0.0)
        g0, g1 = grey(p[0]), grey(p[1])
        d0 = float((g0 & (p[0][:, :, 0] < 0.5)).mean())
        d1 = float((g1 & (p[1][:, :, 0] < 0.5)).mean())
        ok = 0 < n <= 1500 and d1 - d0 >= 0.001
        self.verdict('objlod', ok, 'casters=%d dark %.4f -> %.4f (want casters in (0,1500], +0.001)' % (n, d0, d1))
        # FIXGATES1 finding: BATCH1's objlod arm turned 81 % of the frame (all the near ground) dark; say it every run
        lo = slice(p[0].shape[0] * 3 // 4, None)
        self.info('objlod_near_ground', 'bottom quarter dark %.3f -> %.3f (the loaded cells\' own ground)' % (
            float((g0[lo] & (p[0][lo, :, 0] < 0.5)).mean()), float((g1[lo] & (p[1][lo, :, 0] < 0.5)).mean())))

    # -- pen: the disc penumbra widens the soft band: partial pixels (0.1..0.9) >= 1.5x
    def pen(self):
        p = self.pair('c_v2_none', 'c_v2_pen')
        if p is None:
            return
        # FIXGATES1: grey pixels only (the sky's blue read as "partial" in BATCH1); and the view must hold terrain shadow
        f0 = float((grey(p[0]) & (p[0][:, :, 0] > 0.1) & (p[0][:, :, 0] < 0.9)).mean())
        f1 = float((grey(p[1]) & (p[1][:, :, 0] > 0.1) & (p[1][:, :, 0] < 0.9)).mean())
        sh = float((grey(p[0]) & (p[0][:, :, 0] < 0.3)).mean())
        self.verdict('pen', sh >= 0.02 and f1 >= 1.5 * max(f0, 1e-6),
                     'partial %.4f -> %.4f ratio=%.2f (want >= 1.5) shadowed %.3f (want >= 0.02)' % (f0, f1, f1 / max(f0, 1e-6), sh))

    def terr_mask(self):
        if not self.have('c_v2_none.png'):
            return None, None
        a = img(self.f('c_v2_none.png'))
        g, v = grey(a), a[:, :, 0]
        return g & (v < 0.3), g & (v > 0.99)

    # -- dim: FO4CS's ambient dim in terrain shadow: the median luminance there drops >= 15 %; lit pixels move <= 2 %
    def dim(self):
        p = self.pair('c_fin_none', 'c_fin_dim')
        sh, lit = self.terr_mask()
        if p is None or sh is None or sh.shape != p[0].shape[:2]:
            return
        l0, l1 = lum(p[0]), lum(p[1])
        m = sh & (l0 > 0.02)
        drop = 1.0 - float(np.median(l1[m])) / max(float(np.median(l0[m])), 1e-6) if m.any() else 0.0
        lch = float(np.median(np.abs(l1[lit] - l0[lit]) / np.maximum(l0[lit], 0.02))) if lit.any() else 1.0
        ok = drop >= 0.15 and lch <= 0.02 and m.sum() >= 0.02 * m.size
        self.verdict('dim', ok, 'shadow drop=%.1f%% (>= 15) lit change=%.2f%% (<= 2) shadow px=%d (>= 2 %% of the frame)' % (
            100 * drop, 100 * lch, int(m.sum())))

    # -- contact: FO4CS's march (echo: par 1; FIXGATES1: 192 samples on the half-res mask = FO4CS's 384-pixel reach, the
    # early exit on), the contact view holds contact shadow (grey < 0.5 >= 0.1 %: BATCH1's view 3 was all white, the
    # mask never reached fo4_default) and changes with the item
    def contact(self):
        p = self.pair('c_v3_none', 'c_v3_contact')
        if p is None:
            return
        e = self.uss('c_v3_contact')
        s = num(r'samples=(\d+)', e)
        par = num(r'samples=\d+ .*? par=(\d)', e)
        ex = num(r'samples=\d+ .*? exit=(\d)', e)
        mw, fw = num(r'mask=(\d+)x', e), num(r'full=(\d+)x', e)
        half = mw is not None and fw is not None and mw == (fw + 1) // 2
        sh = float((grey(p[1]) & (p[1][:, :, 0] < 0.5)).mean())
        d = float(np.abs(p[0] - p[1]).mean())
        ok = s == 192 and par == 1 and ex == 1 and half and sh >= 0.001 and d > 0.0005
        self.verdict('contact', ok, 'samples=%s par=%s exit=%s half=%s shadowed=%.4f mean diff=%.4f (want 192, 1, 1, True, >= 0.001, > 0.0005)' % (
            s, par, ex, half, sh, d))
        # the early exit is exact: the march with every sample gives the same picture byte for byte
        q = self.f('c_v3_contact_noexit.png')
        if os.path.isfile(q):
            same = open(q, 'rb').read() == open(self.f('c_v3_contact.png'), 'rb').read()
            ne = num(r'samples=\d+ .*? exit=(\d)', self.uss('c_v3_contact_noexit'))
            self.verdict('contact_exit_exact', same and ne == 0, 'byte-identical=%s noexit echo exit=%s (want True, 0)' % (same, ne))
        q = self.pair('c_v3_contact_full', 'c_v3_contact')
        if q is not None:
            self.info('contact_full_vs_half', 'mean diff %.4f, px > 8/255: %d (the half-res look change; not the default until seen)' % (
                float(np.abs(q[0] - q[1]).mean()), int((np.abs(q[0] - q[1]).max(axis=2) > 8 / 255).sum())))

    # -- fogtail: Volumetric Air's far tail reads the terrain map. FIXGATES1: the fog rule's place (Memory Den, night,
    # CommonwealthFoggy, moon item on in both arms): the item must change >= 1000 px, the changed pixels darken >= 3 %
    # (median), and the picture never brightens (mean +0.2 % at most). The tail only reaches surfaces past the froxels'
    # far edge (17578 u there), so few pixels may qualify: a gate with nothing to judge FAILS, it never passes empty
    def fogtail(self):
        p = self.pair('c_air_none', 'c_air_fogtail')
        if p is None:
            return
        l0, l1 = lum(p[0]), lum(p[1])
        m = np.abs(p[1] - p[0]).max(axis=2) > 0.5 / 255
        n = int(m.sum())
        drop = 1.0 - float(np.median(l1[m])) / max(float(np.median(l0[m])), 1e-6) if n else 0.0
        bright = float(l1.mean()) / max(float(l0.mean()), 1e-6) - 1.0
        e = self.uss('c_air_fogtail')
        live = re.search(r' par=\d+ \[[^\]]*\bfogtail\b', e) is not None
        self.verdict('fogtail', n >= 1000 and drop >= 0.03 and bright <= 0.002 and live,
                     'changed px=%d (>= 1000) drop=%.1f%% (>= 3) mean change=%+.2f%% (<= +0.2) echo live=%s' % (
                         n, 100 * drop, 100 * bright, live))

    # -- cphase / temporal: not measurable from a still frame; the echo says whether the item is live
    def echoed(self):
        for tag, name in (('perf_cphase', 'cphase'), ('perf_temporal', 'temporal')):
            e = self.uss(tag)
            if e:
                live = re.search(r' par=\d+ \[[^\]]*\b%s\b' % name, e) is not None
                bl = num(r'blended=(\d)', e)
                self.info(name, 'UNGATED (still frame): item live in the echo=%s blended=%s' % (live, bl))

    # -- perf: GPU ms at 1080p (median); the items together must add <= 0.8 ms over the base (the shadow budget)
    def perf(self):
        def ms(tag):
            return num(r'gpums median=([\d.]+)', echo(self.f(tag + '.png.gpums.txt')))
        base = ms('perf_base')
        if base is None:
            return
        rows = ['rung=%s base=%.3f' % (ms('perf_rung'), base)]
        for tag in sorted(t[:-4] for t in os.listdir(self.out) if t.startswith('perf_') and t.endswith('.png')):
            if tag in ('perf_base', 'perf_rung'):
                continue
            v = ms(tag)
            ref = ms('perf_base_air') if tag in ('perf_fogtail', 'perf_all') else \
                ms('perf_base_taa') if tag in ('perf_cphase', 'perf_temporal') else base
            if v is not None and ref is not None:
                rows.append('%s=%.3f (%+.3f)' % (tag[5:], v, v - ref))
        self.info('perf', ' '.join(rows))
        cf, ch = ms('perf_contact_full'), ms('perf_contact')
        if cf is not None and ch is not None:
            self.info('perf_contact_saving', 'full-res every-sample %.3f -> half-res early-exit %.3f ms (%+.3f)' % (cf, ch, ch - cf))
        a, ra = ms('perf_all'), ms('perf_base_air')
        if a is not None and ra is not None:
            self.verdict('perf_budget', a - ra <= 0.8, 'all items %+.3f ms over the base (budget 0.8)' % (a - ra))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--red', default='none')
    a = ap.parse_args()
    j = Judge(a.out, a.red)
    for g in (j.off, j.dist, j.cradius, j.near, j.tslope, j.lowsun, j.moon, j.cslope,
              j.objlod, j.pen, j.dim, j.contact, j.fogtail, j.echoed, j.perf):
        try:
            g()
        except Exception as ex:  # a broken picture is a FAIL, never a skip
            j.verdict(g.__name__, False, 'judge error %r' % ex)
    if j.ran == 0:
        j.verdict('ran', False, 'no gate found its pictures')
    print('ALL PASS' if j.ok else 'FAIL', flush=True)
    if a.red != 'none':
        print('red %s: %s' % (a.red, 'FAILS as it must' if not j.ok else 'DID NOT FAIL (the gate is blind)'), flush=True)


if __name__ == '__main__':
    main()
