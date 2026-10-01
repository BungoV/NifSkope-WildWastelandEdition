#!/usr/bin/env python3
"""The cell lights' shadow check (lane SHADOW1, 2026-10-01; tests/spells/cell_shadow.sh).

  cell_shadow_check.py <shot dir> <interior EDID>

Reads <cell>.probe2/3/4/7.png, <cell>.probe2.notes (the "cell lighting ... center=" line), <cell>.shadow.txt
(the shadow echo: the lights in slots 0..2, position, radius, kind, near clip, aim) and <cell>.psp (the cell's
probe soup, the bake's triangles). Nothing shared with the C++: at sampled clean pixels (position from probes
2 + 3, normal from 4) it casts its OWN ray (Moller-Trumbore over the soup, tests/spells/probe_place.py's
Tracer) from the point, lifted 2 units along its normal, toward each slot's light, stopping at the light's near
clip (nearer casters cast nothing). Probe 7 holds the renderer's factor per slot (2/255 + f x 253/255, 0 = out
of reach or facing away). Only pixels THIS script finds in reach and facing the light are read: a 0 can read
1..11 where effect meshes' glow spills over the probe (the first run's "slot 0 all shadowed" was that spill).
Pixels the renderer calls decisive (f under 0.25 or over 0.75) are compared:
  shadowed agree = of the re-traced shadowed points, the share the renderer shadows   (>= 85%, >= 40 of them)
  lit agree      = of the re-traced lit points, the share the renderer lights         (>= 85%, >= 40)
(40 of 40 agreeing puts the true share over 92% at 95% confidence; Solomon's frame holds only ~50 shadowed.)
Hemisphere lights: points behind the plane are unlit in both (the renderer returns 0 there).
One verdict line; exit 0 on PASS. "noshadow" (the red) reads every factor as 1: its shadowed agree is 0.
"""
import os
import re
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_place import Tracer, read_soup  # noqa: E402

PER_SLOT = 4000


def main(shots, cell):
    img = {p: np.asarray(Image.open(os.path.join(shots, '%s.probe%d.png' % (cell, p))).convert('RGB'), float)
           for p in (2, 3, 4, 7)}
    notes = open(os.path.join(shots, cell + '.probe2.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        return 'shadow FAIL %s: no "cell lighting ... center=" line in the notes' % cell
    center = np.array([float(v) for v in m.groups()])
    echo = open(os.path.join(shots, cell + '.shadow.txt'), encoding='utf-8').read()
    slots = {}
    for k, vals in re.findall(r'slot(\d)=([-\d.,]+)', echo):
        v = [float(x) for x in vals.split(',')]
        slots[int(k)] = {'p': np.array(v[0:3]), 'r': v[3], 'kind': int(v[4]), 'near': v[5], 'aim': np.array(v[6:9])}
    if not slots:
        return 'shadow FAIL %s: no shadow slot in the echo (%s)' % (cell, echo.strip()[:120])
    tris, _ = read_soup(os.path.join(shots, cell + '.psp'))

    q = np.round(img[2]) * 256 + np.round(img[3])
    P = q + 0.5 - 32768.0 + center
    N = img[4] / 255.0 * 2 - 1
    nlen = np.linalg.norm(N, axis=2)
    ok = np.abs(nlen - 1) < 0.06
    for k in (2, 3, 4):
        ok &= np.any(img[k] != img[k][0, 0], axis=2)
    ok &= np.any(img[2] != img[3], axis=2) | np.any(img[3] != img[4], axis=2)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    rng = np.random.default_rng(7)
    sh_n = sh_ok = lit_n = lit_ok = edge = 0
    per = []
    for k, L in sorted(slots.items()):
        f = img[7][:, :, k]
        # in reach and facing the light, by this script's own measure: a pixel's 0 ("out of reach") can read
        # 1..11 where effect meshes' glow spills over the probe, and that is not a shadow factor
        to = L['p'] - P
        dl = np.linalg.norm(to, axis=2)
        facing = np.einsum('yxc,yxc->yx', N, to) / np.maximum(dl * np.maximum(nlen, 1e-6), 1e-6)
        mask = ok & (f > 0.5) & (dl < L['r'] - 2.0) & (facing > 0.08)
        ys, xs = np.nonzero(mask)
        if not len(ys):
            per.append('slot%d no pixels' % k)
            continue
        pick = rng.choice(len(ys), size=min(PER_SLOT, len(ys)), replace=False)
        # only the soup's triangles that can sit between a point in reach and the light
        c = tris.mean(axis=1)
        rad = np.linalg.norm(tris - c[:, None, :], axis=2).max(axis=1)
        near = np.linalg.norm(c - L['p'], axis=1) < L['r'] + rad + 4
        tr = Tracer(tris[near])
        a_sh = a_ok = b_n = b_ok = 0
        for i in pick:
            y, x = ys[i], xs[i]
            g = (f[y, x] - 2.0) / 253.0
            if 0.25 <= g <= 0.75:
                edge += 1
                continue
            p = P[y, x]
            n = N[y, x] / nlen[y, x]
            a = p + n * 2.0
            to = L['p'] - a
            d = float(np.linalg.norm(to))
            if L['kind'] == 2 and float(np.dot(p - L['p'], L['aim'])) < 0:
                shadowed = True
            elif d <= L['near'] + 1.0:
                shadowed = False
            else:
                b = L['p'] - to / d * L['near']
                shadowed = tr.ray(a, b) is not None
            if shadowed:
                a_sh += 1
                a_ok += g < 0.25
            else:
                b_n += 1
                b_ok += g > 0.75
        sh_n += a_sh
        sh_ok += a_ok
        lit_n += b_n
        lit_ok += b_ok
        per.append('slot%d kind %d r %.0f: shadowed %d/%d lit %d/%d' % (k, L['kind'], L['r'], a_ok, a_sh, b_ok, b_n))
    sh_share = sh_ok / sh_n if sh_n else 0.0
    lit_share = lit_ok / lit_n if lit_n else 0.0
    verdict = sh_n >= 40 and sh_share >= 0.85 and lit_n >= 40 and lit_share >= 0.85
    return ('shadow %s %s: shadowed agree %.1f%% of %d, lit agree %.1f%% of %d, %d edge pixels skipped; %s'
            % ('PASS' if verdict else 'FAIL', cell, 100 * sh_share, sh_n, 100 * lit_share, lit_n, edge, '; '.join(per)))


if __name__ == '__main__':
    line = main(sys.argv[1], sys.argv[2])
    print(line)
    sys.exit(0 if ' PASS ' in line else 1)
