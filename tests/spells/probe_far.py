#!/usr/bin/env python3
"""The far map's gate (lane PRTPFAR, 2026-10-01; docs/PRTP_PLAN.md 2h).

  probe_far.py <NifSkope.exe> <world.lodl> <world.lodi> <workdir> [--cells x0,y0,x1,y1] [--ref <prtp_reference.exe>]
               [--red shift|hoist|manifest]

Bakes the far map over a block of cells, then:
  heights  -- every .lodl sample a cell reads lies in that cell's stored lo..hi (the census count is 0)
  probes   -- one per cell of the block, each in its own cell's middle
  roofline -- each probe stands at least the hoist over every soup vertex inside its cell (ground, water
              and building boxes), so it really is above the roofs
  colour   -- every ground quad took its colour from the sheet
  check    -- probe_bake.py's structure and budget checks on the baked folder
  ref      -- prtp_reference.py: the links reconstruct what a brute-force tracer sees (with --ref)
`--red shift` reads the heights one cell off; `--red hoist` hoists the probes 2000 units BELOW the
roofline; `--red manifest` drops far.txt (the file square). Each must FAIL. One verdict line; exit 0 on PASS.
"""
import os
import re
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_place import read_soup  # noqa: E402

HOIST = 512.0   # src/probefar.h ProbeFarSpec::hoist
CELL = 4096.0


def main(a):
    exe, lodl, lodi, work = a[0], a[1], a[2], a[3]
    cells = a[a.index('--cells') + 1] if '--cells' in a else '-20,12,-10,22'
    ref = a[a.index('--ref') + 1] if '--ref' in a else ''
    red = a[a.index('--red') + 1] if '--red' in a else ''
    x0, y0, x1, y1 = (int(v) for v in cells.split(','))
    tag = 'far' + ('_' + red if red else '')
    out, soup, pts = (os.path.join(work, tag + s) for s in ('', '.psp', '.probes.txt'))
    os.makedirs(work, exist_ok=True)
    for f in os.listdir(out) if os.path.isdir(out) else []:
        if f.endswith('.tbk'):
            os.remove(os.path.join(out, f))
    cmd = [exe, '-no-gui', 'probefar', '--lodl', lodl, '--lodi', lodi, '--cells', cells, '--out', out,
           '--soup-out', soup, '--probes-out', pts]
    if red == 'shift':
        cmd += ['--red', 'shift']
    if red == 'hoist':
        cmd += ['--hoist', str(HOIST - 2000.0)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=1800)
    if r.returncode != 0:
        print('far FAIL: probefar rc %d %s' % (r.returncode, (r.stderr or r.stdout).strip()[:300]))
        return 1
    txt = r.stdout
    fails = []
    m = re.search(r'outside their cell\'s stored range (\d+)', txt)
    outside = int(m.group(1)) if m else -1
    if outside != 0:
        fails.append('heights: %d samples outside their cell\'s range' % outside)
    m = re.search(r'ground quads (\d+) \(colour from the sheet (\d+)\)', txt)
    quads, coloured = (int(m.group(1)), int(m.group(2))) if m else (0, -1)
    if coloured != quads:
        fails.append('colour: %d of %d quads from the sheet' % (coloured, quads))

    P = np.loadtxt(pts, ndmin=2)
    want = (x1 - x0 + 1) * (y1 - y0 + 1)
    if len(P) != want:
        fails.append('probes: %d for %d cells' % (len(P), want))
    pc = np.floor(P[:, :2] / CELL).astype(int)
    mid = (pc + 0.5) * CELL
    if np.abs(P[:, :2] - mid).max() > 1e-2:
        fails.append('probes: not at their cells\' middles')
    if len({tuple(c) for c in pc}) != len(P):
        fails.append('probes: two in one cell')

    tris, _ = read_soup(soup)
    v = tris.reshape(-1, 3)
    vc = np.floor(v[:, :2] / CELL).astype(int)
    # a vertex on a cell's east/north edge belongs to both; take each cell's top over its closed square
    top = {}
    for dx in (0, -1):
        for dy in (0, -1):
            on = np.ones(len(v), bool)
            if dx:
                on &= np.isclose(v[:, 0], vc[:, 0] * CELL)
            if dy:
                on &= np.isclose(v[:, 1], vc[:, 1] * CELL)
            for (cx, cy), z in zip(map(tuple, vc[on] + (dx, dy)), v[on, 2]):
                if z > top.get((cx, cy), -1e30):
                    top[(cx, cy)] = z
    gaps = np.array([P[i, 2] - top.get(tuple(pc[i]), -1e30) for i in range(len(P))])
    worst = float(gaps.min())
    if worst < HOIST - 1.0:
        fails.append('roofline: a probe stands %.0f over its cell\'s top (want >= %.0f)' % (worst, HOIST))

    if red == 'manifest':   # the far folder without its stated square: the check must refuse it
        os.remove(os.path.join(out, 'far.txt'))
    r2 = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'probe_bake.py'), 'check', out],
                        capture_output=True, text=True)
    chk = r2.stdout.strip().splitlines()[-1] if r2.stdout.strip() else r2.stderr.strip()[:200]
    if r2.returncode != 0:
        fails.append('structure: ' + chk)
    refl = 'reference not run'
    if ref:
        r3 = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'prtp_reference.py'), soup, out,
                             '--probes', '48', '--ref', ref], capture_output=True, text=True)
        refl = r3.stdout.strip().splitlines()[-1] if r3.stdout.strip() else r3.stderr.strip()[:200]
        if r3.returncode != 0:
            fails.append('reference: ' + refl[:160])
    print('far %s%s: %d probes over %d cells, %d triangles; heights outside %d; colour %d/%d; roofline worst %.0f '
          '(hoist %.0f); %s; %s; %s'
          % ('PASS' if not fails else 'FAIL', ' [red ' + red + ']' if red else '', len(P), want, len(tris), outside,
             coloured, quads, worst, HOIST, chk[:90], refl[:120], '; '.join(fails) if fails else 'sound'))
    return 0 if not fails else 1


if __name__ == '__main__':
    if len(sys.argv) < 5:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
