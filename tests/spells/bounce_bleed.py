"""lane BOUNCE1: the colour-bleed box -- a shaded white wall facing a sunlit orange wall, the probe GI's sun bounce
against the brute-force one-bounce reference (tests/bounce_reference.cpp, which shares no code with the bake).

    python bounce_bleed.py <NifSkope.exe> <bounce_reference.exe> <work dir> [--tol 0.15] [--passes 1]

Scene (game units, z up): ground slab z<=0 (linear albedo 0.25 grey); an orange wall x in [-40, 0] facing +X
(0.80 0.36 0.08); a white receiver wall x in [400, 440] facing -X (0.70); the sun toward (0.6, 0, 0.8), white 1.0,
so the orange face is sunlit (n.s 0.6) and the receiver's face (-X) is in its own shade (n.s -0.6). Its shadow lies
on the ground x 25..400, so most of the light the receiver sees is the orange wall's.

Arm A, sun only (--sky 0,0,0): the pipeline's E at the receiver points (the shader's grid sample, cellGiE: the
probes, the voxel grid, its N^2 axis blend, P + N x voxel / 2) against the reference's one-bounce E at the SAME
points. Bars, per point and channel: |E_pipe / E_ref - 1| <= tol (default 0.15) on the mean of the points, and the
hue (r/g, g/b of E) within tol. The receiver then shows alb x E / pi of orange-tinted light (added radiance).
Arm B, a lamp at night (no sun, no sky): an omni lamp at (200, 0, 250), radius 700, warm white, lights the orange
wall and the ground; the receiver's GI is their bounce alone (the lamp's direct light is the shader's), same bars.
Arm C adds a blue weather ambient (0.10 0.15 0.25) to arm A and states the hue shift a viewer sees: the receiver's
light with the GI (bounce + the visible sky) against the flat ambient it replaces.
"""
import os
import struct
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_place import box  # noqa: E402
from probe_bake import write_soup  # noqa: E402
from cell_rooms_check import read_grid, read_rooms, gi_sample  # noqa: E402

SUN_TO = (0.6, 0.0, 0.8)
SUN = (1.0, 1.0, 1.0)
SKY_B = (0.10, 0.15, 0.25)
ORANGE = (0.80, 0.36, 0.08)
WHITE = (0.70, 0.70, 0.70)
GROUND = (0.25, 0.25, 0.25)
LAMP = (200.0, 0.0, 250.0, 700.0, 1.0, 0.8, 0.6)


def scene():
    T, A = [], []
    for tris, alb in ((box(-800, -800, -20, 1200, 800, 0), GROUND), (box(-40, -400, 0, 0, 400, 500), ORANGE),
                      (box(400, -400, 0, 440, 400, 500), WHITE)):
        for t in tris:
            T.append([c for v in t for c in v])
            A.append(tuple(int(round(a * 255)) for a in alb))
    return T, A


def points():
    # the receiver's face (x = 400, facing -X), a 3 x 3 patch in its middle
    return [(399.5, y, z, -1.0, 0.0, 0.0) for y in (-200.0, 0.0, 200.0) for z in (120.0, 250.0, 380.0)]


def run_pipe(exe, soup, out, passes, sun, sky, lamp=None):
    cmd = [exe, '-no-gui', 'probegi', '--soup', soup, '--rect', '-700,-700,1100,700', '--out', out, '--spacing', '140',
           '--rays', '1024', '--passes', str(passes), '--sun', '%g,%g,%g,%g,%g,%g' % sun, '--sky', '%g,%g,%g' % sky]
    if lamp:
        cmd += ['--light', ','.join('%g' % v for v in lamp)]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit('probegi exit %d: %s %s' % (p.returncode, p.stdout[-400:], p.stderr[-400:]))
    G = read_grid(os.path.join(out, 'gi_grid.bin'), os.path.join(out, 'gi_slots.bin'))
    rp = os.path.join(out, 'gi_rooms.bin')
    R = read_rooms(rp) if os.path.exists(rp) else None   # the shader reads the surface's room (lane ROOMCLAMP1)
    E = []
    for q in points():
        s = gi_sample(G, R, q[:3], q[3:], fill=False)
        E.append(np.maximum(s[:3], 0.0))   # cellGiE0 with the sky in the grid: E x share, unnormalized
    return np.array(E), p.stdout


def run_ref(ref, soup, work, tag, rays, sun, lamp=None):
    pts = os.path.join(work, 'pts.txt')
    with open(pts, 'w') as f:
        for q in points():
            f.write('%g %g %g %g %g %g\n' % q)
    out = os.path.join(work, 'ref_%s.tsv' % tag)
    cmd = [ref, soup, pts, str(rays), *map(str, sun), '100000', out]
    if lamp:
        lp = os.path.join(work, 'lamps.txt')
        with open(lp, 'w') as f:
            f.write(' '.join('%g' % v for v in lamp) + '\n')
        cmd.append(lp)
    subprocess.run(cmd, check=True)
    return np.loadtxt(out, comments='#')[:, 1:4]


def compare(tag, Ep, Er, tol, lines):
    ok = True
    lines.append('arm %s:' % tag)
    for i, q in enumerate(points()):
        lines.append('  y %5.0f z %4.0f  pipe %s  ref %s  ratio %s' % (q[1], q[2], np.round(Ep[i], 4), np.round(Er[i], 4),
                                                                    np.round(Ep[i] / np.maximum(Er[i], 1e-9), 3)))
    mp, mr = Ep.mean(0), Er.mean(0)
    ratio = mp / mr
    hue_p = np.array([mp[0] / mp[1], mp[1] / mp[2]])
    hue_r = np.array([mr[0] / mr[1], mr[1] / mr[2]])
    lines.append('  mean E pipe %s ref %s ratio %s' % (np.round(mp, 4), np.round(mr, 4), np.round(ratio, 3)))
    lines.append('  hue r/g g/b pipe %s ref %s (ratio %s)' % (np.round(hue_p, 3), np.round(hue_r, 3), np.round(hue_p / hue_r, 3)))
    lines.append('  added radiance on the white receiver (alb x E / pi): pipe %s ref %s'
                 % (np.round(np.array(WHITE) * mp / np.pi, 4), np.round(np.array(WHITE) * mr / np.pi, 4)))
    if np.any(np.abs(ratio - 1.0) > tol):
        ok = False
        lines.append('  FAIL: the mean E is off the reference by more than %.0f%%' % (tol * 100))
    if np.any(np.abs(hue_p / hue_r - 1.0) > tol):
        ok = False
        lines.append('  FAIL: the hue is off the reference by more than %.0f%%' % (tol * 100))
    return ok


def main():
    exe, ref, work = sys.argv[1:4]
    tol = float(sys.argv[sys.argv.index('--tol') + 1]) if '--tol' in sys.argv else 0.15
    passes = int(sys.argv[sys.argv.index('--passes') + 1]) if '--passes' in sys.argv else 1
    os.makedirs(work, exist_ok=True)
    soup = os.path.join(work, 'bleed.psp')
    T, A = scene()
    write_soup(soup, T, A)
    sun = SUN_TO + SUN
    dark = (0.0, 0.0, 1.0, 0.0, 0.0, 0.0)
    lines = ['bounce_bleed: %d receiver points, passes %d, tol %.2f' % (len(points()), passes, tol)]
    Ea, census = run_pipe(exe, soup, os.path.join(work, 'sun'), passes, sun, (0.0, 0.0, 0.0))
    ok = compare('A sun', Ea, run_ref(ref, soup, work, 'sun', 8192, sun), tol, lines)
    Eb, _ = run_pipe(exe, soup, os.path.join(work, 'lamp'), passes, dark, (0.0, 0.0, 0.0), LAMP)
    ok = compare('B lamp', Eb, run_ref(ref, soup, work, 'lamp', 8192, dark, LAMP), tol, lines) and ok
    Ec, _ = run_pipe(exe, soup, os.path.join(work, 'sky'), passes, sun, SKY_B)
    gc = Ec.mean(0)
    lines.append('arm C, sun + a sky %s: the receiver light with the GI %s vs the flat ambient %s it replaces '
                 '(r/b %.2f vs %.2f)' % (SKY_B, np.round(gc / np.pi, 4), SKY_B, gc[0] / gc[2], SKY_B[0] / SKY_B[2]))
    lines.append('PASS' if ok else 'FAIL')
    print('\n'.join(lines))
    with open(os.path.join(work, 'bleed.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n' + census)
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
