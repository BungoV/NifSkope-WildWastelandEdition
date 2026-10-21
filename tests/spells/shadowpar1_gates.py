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

    # -- cradius: the cascades' penumbra in world units (High vs Ultra texels differ): the width ratio within 1 +- 0.3
    def cradius(self):
        r = {}
        for arm in ('base', 'on'):
            w = {}
            for t in (3, 4):
                p = self.f('crad_%d_%s.png' % (t, arm))
                if not os.path.isfile(p):
                    return
                a = lum(img(p))
                upp = 2.0 * 700.0 / a.shape[1]
                col = int(round(a.shape[1] / 2 + (-700.0 + 500.0) / upp))   # x = -700: inside the run of the shadow
                prof = a[:, col - 2:col + 3].mean(axis=1)
                prof = (prof - prof.min()) / max(prof.max() - prof.min(), 1e-6)
                ew = edge_width(prof)
                w[t] = None if ew is None else ew * upp
            if None in w.values():
                self.verdict('cradius', False, '%s: no shadow found (%s)' % (arm, w))
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

    # -- near: the side edge of the cube's terrain shadow (tier 1) is at most 0.6 x the far map's alone
    def near(self):
        if not self.have('near_base.png', 'near_on.png'):
            return
        w = {}
        for arm in ('base', 'on'):
            _, a, upp, px = self.terr_run('near_' + arm, 2000.0, -1400.0)
            col = px(-1500.0)
            prof = a[:, col - 2:col + 3].mean(axis=1)
            ew = edge_width(prof)
            w[arm] = None if ew is None else ew * upp
        if None in w.values():
            self.verdict('near', False, 'no shadow found %s' % w)
            return
        self.verdict('near', w['on'] <= 0.6 * w['base'], 'edge on=%.0f u base=%.0f u (tolerance on <= 0.6 base)' % (w['on'], w['base']))

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
        d0 = float((p[0][:, :, 0] < 0.5).mean())
        d1 = float((p[1][:, :, 0] < 0.5).mean())
        ok = 0 < n <= 1500 and d1 - d0 >= 0.001
        self.verdict('objlod', ok, 'casters=%d dark %.4f -> %.4f (want casters in (0,1500], +0.001)' % (n, d0, d1))

    # -- pen: the disc penumbra widens the soft band: partial pixels (0.1..0.9) >= 1.5x
    def pen(self):
        p = self.pair('c_v2_none', 'c_v2_pen')
        if p is None:
            return
        f0 = float(((p[0][:, :, 0] > 0.1) & (p[0][:, :, 0] < 0.9)).mean())
        f1 = float(((p[1][:, :, 0] > 0.1) & (p[1][:, :, 0] < 0.9)).mean())
        self.verdict('pen', f1 >= 1.5 * max(f0, 1e-6), 'partial %.4f -> %.4f ratio=%.2f (want >= 1.5)' % (f0, f1, f1 / max(f0, 1e-6)))

    def terr_mask(self):
        if not self.have('c_v2_none.png'):
            return None, None
        v = img(self.f('c_v2_none.png'))[:, :, 0]
        return v < 0.3, v > 0.99

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
        self.verdict('dim', drop >= 0.15 and lch <= 0.02, 'shadow drop=%.1f%% (>= 15) lit change=%.2f%% (<= 2) shadow px=%d' % (
            100 * drop, 100 * lch, int(m.sum())))

    # -- contact: FO4CS's 384 samples and thickness 0.005 (echo), and the contact view changes
    def contact(self):
        p = self.pair('c_v3_none', 'c_v3_contact')
        if p is None:
            return
        e = self.uss('c_v3_contact')
        s = num(r'samples=(\d+)', e)
        par = num(r'samples=\d+ .*? par=(\d)', e)
        d = float(np.abs(p[0] - p[1]).mean())
        self.verdict('contact', s == 384 and par == 1 and d > 0.0005, 'samples=%s par=%s mean diff=%.4f (want 384, 1, > 0.0005)' % (s, par, d))

    # -- fogtail: Volumetric Air's far tail reads the terrain map: inside terrain shadow the picture darkens >= 3 %
    # (median), and the whole picture never brightens (mean +0.2 % at most)
    def fogtail(self):
        p = self.pair('c_air_none', 'c_air_fogtail')
        sh, _ = self.terr_mask()
        if p is None or sh is None or sh.shape != p[0].shape[:2]:
            return
        l0, l1 = lum(p[0]), lum(p[1])
        m = sh & (l0 > 0.02)
        drop = 1.0 - float(np.median(l1[m])) / max(float(np.median(l0[m])), 1e-6) if m.any() else 0.0
        bright = float(l1.mean()) / max(float(l0.mean()), 1e-6) - 1.0
        self.verdict('fogtail', drop >= 0.03 and bright <= 0.002, 'shadow drop=%.1f%% (>= 3) mean change=%+.2f%% (<= +0.2)' % (
            100 * drop, 100 * bright))

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
