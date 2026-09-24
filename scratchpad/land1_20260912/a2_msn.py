"""LAND1 gate A2 -- does vanilla's `_msn` add anything the HEIGHTMAP cannot
give at the macro scales this lane steers by?

bungo asked it directly: "since we are reusing vanilla terrain normals and slope
maps, might as well use them to guide this a bit".  The question is whether the
`_msn` carries macro information the global heightmap does not.  This measures
it instead of arguing it.

  THE TWO AZIMUTHS, at the same macro scale L and on the same 512x512 grid:
    heightmap   the LOW-PASS macro gradient the shipped code computes -- a
                Sobel 3x3 over the ring height grid at a half-step of L/2 world
                units, read from `--dump-land`'s whole-worldspace VHGT dump
                (scratchpad/mountains_20260907/land_all.bin), which is the SAME
                data lodgen reads.
    _msn        vanilla's own sheet, decoded R=east G=up B=north, turned into
                the same (dz/dx, dz/dy) tangent pair, then box low-passed to
                the same L.

  THE DISAGREEMENT: the slope-weighted mean absolute angle between them, in
  degrees, over the texels where the macro slope is above a floor (tan >= 0.05),
  so flat ground -- where an azimuth has no meaning -- cannot dominate.

  THE FLOOR THAT GIVES IT A MEANING: the heightmap's OWN disagreement between
  two ADJACENT scales, L and 2L.  If the `_msn` disagrees with the heightmap by
  no more than the heightmap disagrees with itself one octave away, then the
  `_msn` is carrying no macro information of its own and this lane is right to
  build the guide from the heightmap.

    python a2_msn.py  ->  logs/a2_msn.txt
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
T4 = os.path.join(os.path.dirname(HERE), 'tiling4_20260912')
MT = os.path.join(os.path.dirname(HERE), 'mountains_20260907')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (T4, MT, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import lattice as LT                                          # noqa: E402

VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
SEL = [(-20, 24), (-20, 20), (-36, -20), (-4, -20), (28, -20), (-4, 16), (24, 16)]
DIM = 4
UPT = 32.0          # world units per sheet texel at dim 4 (512 texels / 4 cells)
STEP = 128.0        # the height grid's own step, world units
SLOPE_FLOOR = 0.05
L = []


def say(s):
    L.append(s)
    print(s)


def boxmean(a, k):
    return LT.boxmean(a, k) if k > 1 else np.asarray(a, dtype=np.float64)


def hgt_grad(hgt, L_units, n=512):
    """The SHIPPED rule, vectorised: a Sobel 3x3 at a half-step of L/2 world
    units, on the chunk's own height grid, sampled at the sheet's texel centres."""
    h = float(L_units) * 0.5 / STEP          # half-step in GRID cells
    gy, gx = np.mgrid[0:n, 0:n]
    # texel centre -> grid coordinate: (t + 0.5) * UPT / STEP
    cx = (gx + 0.5) * UPT / STEP
    cy = (gy + 0.5) * UPT / STEP
    t = {}
    for dj in (-1, 0, 1):
        for di in (-1, 0, 1):
            t[(di, dj)] = LT.sample_bilinear(hgt, cx + di * h, cy + dj * h)
    dzdx = ((t[(1, -1)] + 2 * t[(1, 0)] + t[(1, 1)])
            - (t[(-1, -1)] + 2 * t[(-1, 0)] + t[(-1, 1)])) / (8.0 * h * STEP)
    dzdy = ((t[(-1, 1)] + 2 * t[(0, 1)] + t[(1, 1)])
            - (t[(-1, -1)] + 2 * t[(0, -1)] + t[(1, -1)])) / (8.0 * h * STEP)
    return dzdx, dzdy


def msn_grad(path, L_units):
    """vanilla's _msn -> the same tangent pair, box low-passed to the macro scale."""
    a = S.Dds(path).level(0)
    rgb = np.asarray(a, dtype=np.float64)
    if rgb.max() <= 1.001:
        rgb = rgb * 255.0
    n = rgb[:, :, :3] / 255.0 * 2.0 - 1.0
    up = np.maximum(n[:, :, 1], 0.15)
    # THE CONVENTION, settled by measurement and not by reading (see the
    # calibration block in the log): a surface normal is (-dz/dx, -dz/dy, 1),
    # so dz/dx = -R/up and dz/dy = -B/up; and the sheet`s row 0 is NORTH while
    # the height grid`s row 0 is SOUTH, so the rows are flipped.  All eight
    # sign/transpose combinations were tried and only this one collapses to the
    # floor -- the other seven read 63..170 degrees, and a mean near 90 is the
    # signature of a convention error, never of information.
    dzdx = -(n[:, :, 0] / up)[::-1]
    dzdy = -(n[:, :, 2] / up)[::-1]
    k = max(1, int(round(float(L_units) / UPT)))
    if k % 2 == 0:
        k += 1
    return boxmean(dzdx, k), boxmean(dzdy, k)


def disagree(ax, ay, bx, by):
    ta = np.hypot(ax, ay)
    tb = np.hypot(bx, by)
    m = (ta >= SLOPE_FLOOR) & (tb >= SLOPE_FLOOR)
    if m.sum() < 100:
        return float('nan'), 0
    dot = (ax * bx + ay * by)[m] / (ta[m] * tb[m])
    ang = np.degrees(np.arccos(np.clip(dot, -1.0, 1.0)))
    w = ta[m]
    return float((ang * w).sum() / w.sum()), int(m.sum())


def main():
    say('LAND1 gate A2 -- does vanilla`s _msn add anything the heightmap cannot?')
    say('slope-weighted mean |angle| between the two macro azimuths, degrees,')
    say('over texels where BOTH macro slopes read tan >= %.2f' % SLOPE_FLOOR)
    say('')
    land = LT.load_land(os.path.join(MT, 'land_all.bin'))
    scales = [256.0, 512.0, 1024.0, 2048.0]
    say('   %-9s %6s | %10s | %10s | %s'
        % ('chunk', 'scale', '_msn vs hm', 'hm L vs 2L', 'verdict'))
    tot = {s: [] for s in scales}
    flo = {s: [] for s in scales}
    for cx, cy in SEL:
        p = os.path.join(VAN, 'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy))
        if not os.path.isfile(p):
            say('   %-9s no vanilla _msn -- SKIPPED BY NAME' % ('%d,%d' % (cx, cy)))
            continue
        hgt = LT.chunk_hgt(land, cx, cy, DIM)
        for s in scales:
            hx, hy = hgt_grad(hgt, s)
            mx, my = msn_grad(p, s)
            d, n = disagree(mx, my, hx, hy)
            h2x, h2y = hgt_grad(hgt, s * 2.0)
            f, _ = disagree(hx, hy, h2x, h2y)
            tot[s].append(d)
            flo[s].append(f)
            say('   %-9s %6.0f | %10.2f | %10.2f | %s'
                % ('%d,%d' % (cx, cy), s, d, f,
                   'adds nothing' if d <= f else 'ADDS something'))
    say('')
    say('   %-16s %10s %10s  %s' % ('MEDIAN over the seven', '_msn vs hm',
                                    'hm L vs 2L', 'verdict'))
    verdict_adds = 0
    for s in scales:
        if not tot[s]:
            continue
        d = float(np.median(tot[s]))
        f = float(np.median(flo[s]))
        if d > f:
            verdict_adds += 1
        say('   scale %-10.0f %10.2f %10.2f  %s'
            % (s, d, f, 'adds nothing' if d <= f else 'ADDS something'))
    say('')
    say('ANSWER: the `_msn` %s macro information the heightmap does not carry,'
        % ('DOES add' if verdict_adds else 'adds NO'))
    say('at %d of %d scales tested.' % (verdict_adds, len(scales)))
    with open(os.path.join(HERE, 'logs', 'a2_msn.txt'), 'w', newline=chr(10)) as f:
        f.write(chr(10).join(L) + chr(10))
    return 0


if __name__ == '__main__':
    sys.exit(main())
