"""lane TERRSHADOW1: the judge of tests/spells/terrshadow1_gates.sh (pictures + echoes, never the code under test).

usage: python terrshadow1_gates.py --out DIR [--red NAME]
Prints one verdict line per gate and a last line ALL PASS / FAIL; with --red the run must FAIL.
"""
import argparse
import glob
import math
import os
import re
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ELEVS = (30, 10, 3, 1)
CUBE_H = 512.0          # QryCube01: a 512-unit cube on the origin, the ground at z = -256
SHOT_W, SHOT_H = 1600, 1000
LENGTH_HW = {30: 1200, 10: 3200, 3: 10500, 1: 31000}   # the length shots' ortho half-widths (gates.sh)


def grey(path):
    return np.asarray(Image.open(path).convert('RGB'), dtype=np.float64) / 255.0


def echo(path):
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            return f.read().strip()
    except OSError:
        return ''


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

    def f(self, name):
        return os.path.join(self.out, name)

    def have(self, name):
        return os.path.isfile(self.f(name))

    # -- off: every new row off = the before exe byte for byte
    def off(self):
        for fr in ('v8', 'v8on', 'topon'):
            old = self.f('off_%s_old.png' % fr)
            if not os.path.isfile(old):
                continue
            ref = open(old, 'rb').read()
            for arm in ('new', 'pinned'):
                p = self.f('off_%s_%s.png' % (fr, arm))
                same = os.path.isfile(p) and open(p, 'rb').read() == ref
                e = echo(self.f('off_%s_%s.uss.txt' % (fr, arm)))
                quiet = all(k in e for k in ('cascade=0', 'contact=0', 'terrain=0', 'sunpath=0'))
                self.verdict('off_%s_%s' % (fr, arm), same and quiet, 'byte-identical=%s echo-off=%s' % (same, quiet))

    # -- march: the swept map vs the independent CPU march
    def march(self):
        dumps = [self.f('march_%d.dump' % e) for e in ELEVS if self.have('march_%d.dump' % e)]
        dumps += sorted(glob.glob(self.f('march_*_az*.dump')))
        if not dumps:
            return
        r = subprocess.run([sys.executable, os.path.join(HERE, 'terrshadow1_march.py'), '--ground', '-256'] + dumps,
                           capture_output=True, text=True)
        for ln in r.stdout.strip().splitlines():
            self.verdict('march', ln.startswith('PASS'), ln)
        if r.returncode != 0 and 'FAIL' not in r.stdout:
            self.verdict('march', False, r.stderr.strip()[-300:])
        if self.red == 'none' and len([d for d in dumps if '_az' not in d]) < len(ELEVS):
            self.verdict('march', False, 'only %d of %d dumps' % (len(dumps), len(ELEVS)))
        if self.red == 'none' and len([d for d in dumps if '_az' in d]) < 2:
            self.verdict('march_az', False, 'the off-axis dumps (az 120, 200) are missing')

    # -- length: the terrain view's shadow on the ground vs 512 / tan(elev)
    def length(self):
        for e in ELEVS:
            p = self.f('length_%d.png' % e)
            if not os.path.isfile(p):
                continue
            hw = LENGTH_HW[e]
            cx = -(hw - 600)
            upp = 2.0 * hw / SHOT_W          # world units per pixel
            a = grey(p)[:, :, 0]
            mid = a.shape[0] // 2   # the window may come back shorter than asked (967 of 1000)
            row = a[mid - 2:mid + 3, :].mean(axis=0)

            def px(x):
                return int(round(SHOT_W / 2 + (x - cx) / upp))
            # from the cube's west face (x = -256) walk -x: the shadow is the run below 0.5
            start = max(px(-256.0) - 2, 0)
            end = start
            while end > 0 and row[end] < 0.5:
                end -= 1
            dark0 = row[start] < 0.5
            meas = (start - end + 2) * upp
            want = CUBE_H / math.tan(math.radians(e))
            tol = max(0.05 * want, 1.5 * 128.0)   # the Ultra map's texel and the sun disc's penumbra
            # the side toward the sun must be lit: a flipped sweep shadows the east
            east = row[min(px(256.0) + 4, SHOT_W - 1):min(px(256.0) + 60, SHOT_W)]
            east_lit = east.size > 0 and float(east.min()) > 0.5
            ok = dark0 and abs(meas - want) <= tol and east_lit
            self.verdict('length_%d' % e, ok, 'measured=%.0f analytic=%.0f err=%+.1f%% tol=%.0f east_lit=%s' % (
                meas, want, 100.0 * (meas - want) / want, tol, east_lit))

    # -- handoff: composed == min( faded cascades, terrain ) everywhere, and no step across the fade band
    def handoff(self):
        for e in ELEVS:
            v = {k: grey(self.f('handoff_%d_v%d.png' % (e, k))) for k in (1, 2, 4, 5)
                 if self.have('handoff_%d_v%d.png' % (e, k))}
            if 4 not in v:
                continue
            det = []
            ok = len(v) == 4
            if 1 in v and 2 in v:
                diff = v[4][:, :, 0] - np.minimum(v[1][:, :, 0], v[2][:, :, 0])
                darker = float(np.mean(diff < -3.0 / 255.0))
                lighter = float(np.mean(diff > 3.0 / 255.0))
                ok &= darker + lighter < 0.002
                det.append('composed!=min: double-dark %.3f%% lit-gap %.3f%%' % (100 * darker, 100 * lighter))
            if 5 in v:
                wR = v[5][:, :, 0]
                inMap = v[5][:, :, 1]
                det.append('fade-band px=%d in-map=%.1f%%' % (int(((wR > 0.02) & (wR < 0.98)).sum()),
                                                                100 * float(np.mean(inMap > 0.5))))
                # the seam: a step in the composed picture that neither producer has at that pixel (min of two
                # smooth signals is smooth; a producer's own shadow edge is not a seam). Horizontal and vertical
                # neighbours, every pixel within 8 px of the fade band.
                band = (wR > 0.02) & (wR < 0.98)
                if band.any() and 1 in v and 2 in v:
                    near = band.copy()
                    for k in range(8):
                        near[1:, :] |= near[:-1, :]; near[:-1, :] |= near[1:, :]
                        near[:, 1:] |= near[:, :-1]; near[:, :-1] |= near[:, 1:]
                    c4, c1, c2 = v[4][:, :, 0], v[1][:, :, 0], v[2][:, :, 0]
                    worst = 0.0
                    for ax in (0, 1):
                        d4 = np.abs(np.diff(c4, axis=ax)); d1 = np.abs(np.diff(c1, axis=ax)); d2 = np.abs(np.diff(c2, axis=ax))
                        m = (near[1:, :] if ax == 0 else near[:, 1:]) & (d1 <= 0.02) & (d2 <= 0.02)
                        if m.any():
                            worst = max(worst, float(d4[m].max()))
                    ok &= worst <= 0.04
                    # the terrain must carry the cascades' shadow through the band: unfaded cascade dark there
                    wsafe = np.maximum(wR, 1e-3)
                    casc = 1.0 - (1.0 - c1) / wsafe
                    mid = (wR > 0.1) & (wR < 0.9) & (casc < 0.3)
                    # within one Ultra texel (128 u = ~20 px at this camera: the 512 u cube spans ~80 px), the
                    # rasterized height snaps the caster's outline by up to a texel
                    from scipy.ndimage import binary_dilation
                    tdark = binary_dilation(c2 < 0.5, iterations=20)
                    cover = float(np.mean(tdark[mid])) if mid.sum() >= 50 else 1.0
                    ok &= cover >= 0.99
                    det.append('seam-step=%.3f (producers smooth) cascade-dark-in-band px=%d terrain-covers=%.1f%%' % (
                        worst, int(mid.sum()), 100 * cover))
                else:
                    det.append('no fade band')
            self.verdict('handoff_%d' % e, ok, ' '.join(det))

    # -- cascade: the presets echo their tier, the picture changes with the preset
    def cascade(self):
        pics = {}
        for t in range(5):
            e = echo(self.f('cascade_%d.uss.txt' % t))
            if not e:
                continue
            m = re.search(r'cascade=(\d)', e)
            ok = m is not None and int(m.group(1)) == t
            if self.have('cascade_%d.png' % t):
                pics[t] = grey(self.f('cascade_%d.png' % t))[:, :, 0]
            self.verdict('cascade_%d' % t, ok, 'csm=[%s]' % echo(self.f('cascade_%d.csm.txt' % t))[:200])
        if 0 in pics and 4 in pics:
            d = float(np.mean(np.abs(pics[4] - pics[0])))
            self.verdict('cascade_differs', d > 1e-4, 'mean |Ultra - Off| = %.5f' % d)

    # -- sun: the engine arc vs the real path (the real path checked against the formula here)
    def sun(self):
        rows, ok = [], True
        for h in range(4, 21):
            e = echo(self.f('sun_%d.uss.txt' % h))
            m = re.search(r'engine elev=([-\d.]+) disc_elev=([-\d.]+) \(real would be ([-\d.]+)\)', e)
            if not m:
                continue
            eng, disc, real = map(float, m.groups())
            n = (296 + 0) % 365 + 1                         # game day 0 = October 24
            dec = math.radians(23.44 * math.sin(2 * math.pi * (284 + n) / 365.0))
            ha, phi = math.radians(15.0 * (h - 12.0)), math.radians(42.36)
            ref = math.degrees(math.asin(math.sin(phi) * math.sin(dec) + math.cos(phi) * math.cos(dec) * math.cos(ha)))
            ok &= abs(ref - real) < 0.05
            rows.append('h%d engine=%.2f disc=%.2f real=%.2f (formula %.2f)' % (h, eng, disc, real, ref))
        if rows:
            self.verdict('sun', ok, '\n  ' + '\n  '.join(rows))

    # -- cell: the combined low-sun picture (Concord, sun 3 degrees)
    def cell(self):
        if self.have('cell_off_old.png'):
            same = self.have('cell_off_new.png') and \
                open(self.f('cell_off_old.png'), 'rb').read() == open(self.f('cell_off_new.png'), 'rb').read()
            quiet = all(k in echo(self.f('cell_off_new.uss.txt')) for k in ('cascade=0', 'contact=0', 'terrain=0', 'sunpath=0'))
            if same or not self.have('cell_off_old2.png') or not self.have('cell_off_new.png'):
                self.verdict('cell_off', same and quiet, 'byte-identical=%s echo-off=%s' % (same, quiet))
            else:
                # the cell view is not deterministic run to run (far foliage sways): the old build twice gives the
                # noise floor. A third run moves a few more pixels at the edge of what sways, so the new build may
                # differ only within 3 px of the old-vs-old unstable pixels (at most 0.001% of the picture beyond);
                # a real change of the Off path (a shadow, a tint) moves thousands of pixels away from the foliage
                from scipy.ndimage import binary_dilation
                o1, o2, nw = (np.asarray(Image.open(self.f('cell_off_%s.png' % k)).convert('RGB')).astype(int) for k in ('old', 'old2', 'new'))
                stable = (o1 == o2).all(axis=2)
                off = stable & (nw != o1).any(axis=2)
                far = off & ~binary_dilation(~stable, iterations=3)
                share = 1.0 - stable.mean()
                ok = quiet and int(far.sum()) <= 0.00001 * far.size and share < 0.05
                self.verdict('cell_off', ok, 'echo-off=%s old-vs-old unstable %.2f%% of pixels; new differs on %d stable pixels, '
                             '%d of them beyond 3 px of the unstable ones (max step %d)'
                             % (quiet, 100 * share, int(off.sum()), int(far.sum()), int(np.abs(nw - o1)[off].max()) if off.any() else 0))
        v = {k: grey(self.f('cell_v%d.png' % k))[:, :, 0] for k in range(1, 6) if self.have('cell_v%d.png' % k)}
        if not v:
            return
        ok = len(v) == 5
        det = ['views=%d' % len(v)]
        # the cell must have loaded: the echo shows every producer running on real input, the views differ
        e4 = open(self.f('cell_v4.uss.txt')).read() if self.have('cell_v4.uss.txt') else ''
        live = 'terrain=4 [on N=' in e4 and 'contact=4 [on' in e4 and 'no opaque pass' not in e4
        ok &= live
        det.append('producers live=%s' % live)
        if 1 in v and 2 in v and 4 in v:
            distinct = not np.array_equal(v[1], v[2]) and not np.array_equal(v[2], v[4])
            ok &= distinct
            det.append('views distinct=%s' % distinct)
        if ok:
            w = grey(self.f('cell_v5.png'))
            wR, inMap = w[:, :, 0], w[:, :, 1]
            ground = inMap > 0.5
            diff = v[4] - np.minimum(np.minimum(v[1], v[2]), v[3])
            darker = float(np.mean(diff < -3.0 / 255.0))
            lighter = float(np.mean(diff > 3.0 / 255.0))
            ok &= darker + lighter < 0.002
            det.append('composed!=min: double-dark %.3f%% lit-gap %.3f%%' % (100 * darker, 100 * lighter))
            # the sky draws the same in every view of the ladder; the scene is every other pixel
            rgb = [np.asarray(Image.open(self.f('cell_v%d.png' % k)).convert('RGB')) for k in range(1, 6)]
            sky = np.ones(rgb[0].shape[:2], dtype=bool)
            for k in range(1, 5):
                sky &= (rgb[k] == rgb[0]).all(axis=2)
            scene = ~sky
            ns = max(int(scene.sum()), 1)
            # each producer shows, in the scene: some of it shadowed and some of it lit (a picture all in shadow
            # shows nothing of the handoffs)
            frac = {k: (float((v[k] < 0.5)[scene].sum()) / ns, float((v[k] > 0.8)[scene].sum()) / ns) for k in (1, 2, 4)}
            frac[3] = (float((v[3] < 0.5)[scene].sum()) / ns, float((v[3] > 0.8)[scene].sum()) / ns)
            det.append('scene %.1f%% of the picture; shadowed/lit: cascades %.1f/%.1f%% terrain %.1f/%.1f%% contact %.2f/%.1f%% '
                       'composed %.1f/%.1f%%; terrain map covers %.1f%%' % (
                           100.0 * ns / scene.size, 100 * frac[1][0], 100 * frac[1][1], 100 * frac[2][0], 100 * frac[2][1],
                           100 * frac[3][0], 100 * frac[3][1], 100 * frac[4][0], 100 * frac[4][1], 100 * float(ground.mean())))
            ok &= all(frac[k][0] >= 0.01 and frac[k][1] >= 0.01 for k in (1, 2, 4)) and frac[3][0] >= 0.0005
            # the ridge past the cascades: terrain shadow where the cascades no longer reach (fade weight ~0)
            past = scene & (wR < 0.02) & ground
            np_ = max(int(past.sum()), 1)
            tpast = float(((v[2] < 0.5) & past).sum()) / np_
            cpast = float(((v[4] < 0.5) & past).sum()) / np_
            ok &= int(past.sum()) >= 1000 and tpast >= 0.01 and abs(cpast - tpast) <= 0.01
            det.append('past the cascades: %d px, terrain-shadowed %.1f%%, composed %.1f%%' % (int(past.sum()), 100 * tpast, 100 * cpast))
            # the seam: a step in the composed factor that no producer has at that pixel, within 4 px of the
            # cascades' fade band (a producer's own shadow edge or a silhouette is not a seam)
            band = (wR > 0.02) & (wR < 0.98)
            if band.any():
                from scipy.ndimage import binary_dilation
                nearb = binary_dilation(band, iterations=4)
                seams = []
                for ax in (0, 1):
                    d4 = np.abs(np.diff(v[4], axis=ax))
                    smooth = np.ones_like(d4, dtype=bool)
                    for k in (1, 2, 3):
                        smooth &= np.abs(np.diff(v[k], axis=ax)) <= 0.02
                    m = (nearb[1:, :] if ax == 0 else nearb[:, 1:]) & smooth
                    seams.extend(d4[m].tolist())
                if seams:
                    p999 = float(np.percentile(seams, 99.9))
                    ok &= p999 <= 0.04
                    det.append('seam-step p99.9=%.3f max=%.3f (n=%d, producers smooth)' % (p999, max(seams), len(seams)))
                else:
                    det.append('fade band has no smooth pixels')
            else:
                det.append('no fade band in the picture')
            # contact vs cascades: the contact's dark pixels lie in or beside the cascades' shadow
            cd = (v[3] < 0.6) & scene
            if cd.sum():
                from scipy.ndimage import binary_dilation
                near = binary_dilation((v[1] < 0.6) & scene, iterations=6)
                agree = float((cd & near).sum()) / float(cd.sum())
                ok &= agree >= 0.6
                det.append('contact-in-cascade-shadow %.1f%%' % (100 * agree))
        self.verdict('cell', ok, ' '.join(det))

    # -- perf: the GPU ms + the producers' own timers (a report, read by eye; the budget is in FINAL.md)
    def perf(self):
        rows, over, gms = [], [], {}
        for p in sorted(glob.glob(self.f('perf_*.uss.txt'))):
            tag = os.path.basename(p)[:-len('.uss.txt')]
            e = echo(p)
            g = echo(self.f(tag + '.png.gpums.txt'))
            timers = re.findall(r'(upload_gpu_ms=[-\d.]+|sweep_cpu_ms=[-\d.]+|raster_ms=[-\d.]+|frame_cpu_max_ms=[-\d.]+|'
                                r'upload_cpu_ms=[-\d.]+|sweeps=\d+|valid=\d|gpu_ms=[-\d.]+|mem_mb=[\d.]+|cpu_mb=[\d.]+|'
                                r'cascade=\d|contact=\d|terrain=\d)', e)
            if tag.startswith('perf_terrain_'):
                # the render thread's budget: no frame stalls (< 0.3 ms CPU, every frame), the band's upload
                # < 0.3 ms GPU, the map finished in the run, and one sweep for a pinned sun (no re-sweeps)
                m = dict(re.findall(r'(frame_cpu_max_ms|upload_gpu_ms|sweeps|valid)=([-\d.]+)', e.split('terrain=')[-1]))
                fm, ug = float(m.get('frame_cpu_max_ms', 99)), float(m.get('upload_gpu_ms', 99))
                good = fm <= 0.3 and ug <= 0.3 and m.get('valid') == '1' and m.get('sweeps') == '1'
                if not good:
                    over.append('%s(frame_cpu_max %.3f upload_gpu %.3f valid %s sweeps %s)' % (
                        tag, fm, ug, m.get('valid'), m.get('sweeps')))
            gm = re.search(r'gpums median=([\d.]+)', g)
            if not gm:
                over.append('%s(no gpums)' % tag)
            gms[tag] = float(gm.group(1)) if gm else None
            rows.append([tag, g.strip(), ' '.join(timers)])
        base = gms.get('perf_base')
        rows = ['%-20s frame gpu %s ms (%s vs base) %s' % (
            t, '%.3f' % gms[t] if gms[t] is not None else '-',
            '%+.3f' % (gms[t] - base) if gms[t] is not None and base is not None else '-', tm) for t, g, tm in rows]
        if rows:
            # a report, but only of a loaded cell: a producer with no input measures nothing
            dead = [os.path.basename(p) for p in glob.glob(self.f('perf_*.uss.txt'))
                    if 'no terrain shapes' in echo(p) or 'no opaque pass' in echo(p)]
            self.verdict('perf', not dead and not over, ('dead input: %s ' % ' '.join(dead) if dead else '')
                         + ('over budget: %s' % ' '.join(over) if over else '') + '\n  ' + '\n  '.join(rows))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--red', default='none')
    a = ap.parse_args()
    j = Judge(a.out, a.red)
    for g in (j.off, j.march, j.length, j.handoff, j.cascade, j.sun, j.cell, j.perf):
        try:
            g()
        except Exception as ex:  # a broken judge is a FAIL, never a skip
            j.verdict(g.__name__, False, 'judge error: %r' % ex)
    if j.ran == 0:
        j.verdict('nothing', False, 'no results in %s' % a.out)
    print('ALL PASS' if j.ok else 'FAIL', '(red=%s)' % a.red)
    sys.exit(0 if j.ok else 1)


if __name__ == '__main__':
    main()
