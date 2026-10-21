"""fargrid1_gates.py -- the judge of tests/spells/fargrid1_gates.sh (lane FARGRID1, the far air grid).

Reads the pictures and the "cell air:" echo in each <tag>.log; writes verdict.txt. A gate whose inputs are missing is
MISSING, never PASS. With --red <name> the aimed gate must FAIL (the harness proves the gate can see the defect).
"""
import argparse
import os
import re
import sys

import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument('--out', required=True)
ap.add_argument('--red', default='none')
a = ap.parse_args()
OUT = a.out
lines = []


def f(name):
    return os.path.join(OUT, name)


def img(tag):
    p = f(tag + '.png')
    if not os.path.isfile(p):
        return None
    return np.asarray(Image.open(p).convert('RGB'), dtype=np.float64) / 255.0


def luma(x):
    return x[..., 0] * 0.2126 + x[..., 1] * 0.7152 + x[..., 2] * 0.0722


def echo(tag):
    p = f(tag + '.log')
    if not os.path.isfile(p):
        return ''
    t = open(p, encoding='utf-8', errors='replace').read()
    air = [l for l in t.splitlines() if l.startswith('cell air:')]
    return air[-1] if air else ''


def say(gate, verdict, why):
    lines.append('%-7s %-7s %s' % (gate, verdict, why))


# off: Far Air Off (unset, pinned 0) == the rung, byte for byte
o, n, p = img('off_old'), img('off_new'), img('off_pinned')
if o is not None or n is not None:
    if o is None or n is None or p is None:
        say('off', 'MISSING', 'a picture is missing')
    else:
        dn, dp = int(np.count_nonzero(o != n)), int(np.count_nonzero(o != p))
        say('off', 'PASS' if dn == 0 and dp == 0 else 'FAIL', 'channels differing: unset %d, pinned 0 %d' % (dn, dp))

# leak: the far band of the air's L (the top of the ground half, where the far grid lives) drops with the ridge's shadow
u, s = img('leak_noterr'), img('leak_terr')
if u is not None or s is not None:
    e = echo('leak_terr')
    if u is None or s is None or 'far=on' not in e:
        say('leak', 'MISSING', 'pictures or far=on echo missing (%s)' % (e[-120:] if e else 'no echo'))
    else:
        h = u.shape[0]
        band = slice(int(h * 0.30), int(h * 0.55))	# horizon band at the player-eye camera: the km range
        lu, ls = luma(u[band]), luma(s[band])
        ratio = float(ls.mean() / max(lu.mean(), 1e-6))
        shaded = float(np.mean(ls < lu * 0.8))	# share of the band the terrain darkens by 20 % or more
        terr = re.search(r'terrain=(\d)', e)
        ok = ratio < 0.95 and shaded > 0.05 and terr and terr.group(1) == '1'
        say('leak', 'PASS' if ok else 'FAIL', 'band L terrain/none %.3f, shaded share %.3f, terrain map %s'
            % (ratio, shaded, terr.group(1) if terr else '?'))

# seam: far High vs Off, sun + ambient, no shadows: the same medium, so the air's L agrees
x, y = img('seam_off'), img('seam_on')
if x is not None or y is not None:
    if x is None or y is None:
        say('seam', 'MISSING', 'a picture is missing')
    else:
        lx, ly = luma(x), luma(y)
        d = np.abs(ly - lx) / np.maximum(lx, 1.0 / 255.0)
        mean_rel = float(abs(ly.mean() - lx.mean()) / max(lx.mean(), 1e-6))
        p95 = float(np.percentile(d, 95))
        # the row profile of the ON picture: no step larger than the OFF picture's own largest step + 2 levels
        rx, ry = lx.mean(axis=1), ly.mean(axis=1)
        step = float(np.abs(np.diff(ry)).max() - np.abs(np.diff(rx)).max())
        ok = mean_rel < 0.03 and p95 < 0.08 and step < 2.0 / 255.0
        say('seam', 'PASS' if ok else 'FAIL', 'mean rel %.4f, p95 rel %.4f, row step excess %.5f' % (mean_rel, p95, step))

# stable: a still camera, frames 2 and 3 of one launch: zero difference (the base too: the noise floor)
for tag in ('stable_base', 'stable_far'):
    a2, a3 = img(tag + '_f2'), img(tag + '_f3')
    if a2 is None and a3 is None and img(tag) is None:
        continue
    if a2 is None or a3 is None:
        say('stable', 'MISSING', '%s: frames 2/3 missing' % tag)
    else:
        dif = int(np.count_nonzero(a2 != a3))
        say('stable', 'PASS' if dif == 0 else 'FAIL', '%s: channels differing frame 2 vs 3: %d' % (tag, dif))

# perf: the budget table (echo); judged against the per-preset budgets in notes\fargrid1\STATUS.md
BUDGET = {1: (0.15, 36.0), 2: (0.30, 40.0), 3: (0.60, 44.0), 4: (1.20, 110.0)}	# far build GPU ms, both volumes MB at 1080p
for t in range(5):
    tag = 'perf_%d' % t
    if not os.path.isfile(f(tag + '.log')):
        continue
    e = echo(tag)
    g = ''
    gp = f(tag + '.png.gpums.txt')
    if os.path.isfile(gp):
        m = re.search(r'gpums median=([\d.]+)', open(gp, encoding='utf-8', errors='replace').read())
        g = m.group(1) if m else ''
    fm = re.search(r'far=on\(tier=(\d) (\S+) .*?gpums=([\d.]+) volmb=([\d.]+)', e)
    if t == 0:
        say('perf', 'INFO', 'tier 0: frame gpums median %s; %s' % (g or '?', 'far echo absent (as it must)' if 'far=' not in e else 'FAR ECHO AT TIER 0'))
    elif not fm:
        say('perf', 'MISSING', 'tier %d: no far=on echo' % t)
    else:
        ms, mb = float(fm.group(3)), float(fm.group(4))
        bm, bb = BUDGET[t]
        say('perf', 'PASS' if ms <= bm and mb <= bb else 'FAIL', 'tier %d grid %s: far build %.3f ms (budget %.2f), volumes %.1f MB (budget %.0f), frame median %s'
            % (t, fm.group(2), ms, bm, mb, bb, g or '?'))

if not lines:
    lines.append('nothing judged')
red = a.red
bad = [l for l in lines if ' FAIL ' in l or ' MISSING ' in l]
if red == 'none':
    total = 'PASS' if not bad else 'FAIL'
else:
    aim = {'farnoterr': 'leak', 'farseam': 'seam', 'farjitter': 'stable'}[red]
    caught = any(l.startswith(aim) and ' FAIL ' in l for l in lines)
    total = 'RED CAUGHT (%s FAILED as it must)' % aim if caught else 'RED MISSED: %s did not fail' % aim
text = 'red=%s\n%s\nTOTAL %s\n' % (red, '\n'.join(lines), total)
open(f('verdict.txt'), 'w', encoding='utf-8').write(text)
sys.stdout.write(text)
