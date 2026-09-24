"""ROADS4 item 2/3 -- the ground-material knob, measured on REAL BAKES.

Every row below is read off a sheet the generator wrote at 06:3x on
2026-09-12 with the exe built at 06:31:05; nothing here is simulated.  The
variants are the same command line with one switch added:

    r4_default        (the shipped default: detail 1, ground paint 1)
    r4_gp075 / gp05 / gp025 / gp0     --road-ground-paint 0.75 / 0.5 / 0.25 / 0

and `r4_noroads` is `--no-roads`, the ground under everything.

WHAT IS MEASURED, and against what floor:

  T  the TERRAIN-CLASS texels -- those the projection says are won by a shape
     whose material lives under `materials/Landscape/Ground/` -- their mean
     luminance in each variant against Bethesda's at the same texels.
  E  THE EDGE, the defect bungo named: the mean luminance gradient where such
     a patch meets the road surface INSIDE the road plane, against vanilla at
     the same texels and against the same boundary set displaced five ways.
  P  the cross-road profile's largest second difference (lane ROADS3's F3e /
     this brief's G2), ours against vanilla on the same signed-distance axis.
  W  the painted mask's width, against the PROJECTED road width, which is a
     world fact and therefore the same number for vanilla.
  R  the harness metric `tests/spells/lodgen_roads_metric.py` scores (R5).
"""
import json
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(SCRATCH, 'roads3_20260911'))
import r4lib as R                                              # noqa: E402
import r3lib as R3                                             # noqa: E402

DISPLACE = [(37, 0), (0, 37), (-29, 23), (23, -29), (53, 53)]
VARIANTS = ['r4_default', 'r4_gp075', 'r4_gp05', 'r4_gp025', 'r4_gp0',
            'r4_op083', 'r4_op05', 'r4_op0326']
OUT = {}


def sheet(variant, tile):
    t = R.TILES[tile]
    import splatlib as S
    p = os.path.join(HERE, 'out', variant, tile, 'tex',
                     'Commonwealth.4.%d.%d.DDS' % (t['cx'], t['cy']))
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64)


def grad_mag(a):
    gy, gx = np.gradient(a)
    return np.hypot(gx, gy)


def boundary_of(mask, road):
    edge = np.zeros_like(mask)
    for dj, di in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        nb = np.roll(np.roll(mask, dj, axis=0), di, axis=1)
        edge |= mask & ~nb & np.roll(np.roll(road, dj, axis=0), di, axis=1)
    return edge


def is_ground(mat):
    return 'landscape/ground/' in (mat or '').replace(chr(92), '/').lower()


def run(tile):
    print('')
    print('=' * 78)
    print('CHUNK %s' % tile)
    print('=' * 78)
    ground = sheet('r4_noroads', tile)
    van = R.vanilla_sheet(tile)
    Lg, Lv = R.lum(ground), R.lum(van)
    pr = R.project(tile)
    shp = pr['shp']
    gOfShape = np.array([is_ground(s['mat']) for s in pr['shapes']])
    idx = np.clip(shp, 0, None)
    proj = shp >= 0

    # the terrain-class set is fixed by the DEFAULT bake's winners, so every
    # variant is read on the SAME texels and the rows are comparable.
    L0 = R.lum(sheet('r4_default', tile))
    road0 = R.painted_mask(sheet('r4_default', tile), ground) & proj
    isTerrain = road0 & gOfShape[idx]
    edge = boundary_of(isTerrain, road0)
    gV = grad_mag(Lv)
    print('')
    print('the sets, fixed once on the default bake and used for every row:')
    print('   painted road texels (default)      %6d' % int(road0.sum()))
    print('   of those, terrain-class            %6d  = %.1f%%'
          % (int(isTerrain.sum()), 100.0 * isTerrain.sum() / max(1, road0.sum())))
    print('   terrain-patch boundary             %6d' % int(edge.sum()))
    print('   vanilla at the boundary            %6.3f   (its floor, the same '
          'set displaced 5 ways: %.3f)'
          % (float(gV[edge].mean()),
             float(np.mean([gV[np.roll(np.roll(edge, dj, 0), di, 1)].mean()
                            for dj, di in DISPLACE]))))

    rows = []
    print('')
    print('   %-12s %8s %8s %8s %8s %8s %8s %8s'
          % ('variant', 'painted', 'T mean', 'T-van', 'E ours', 'E floor',
             'P ours', 'W mean') + ' %8s %8s' % ('S mean', 'S-T'))
    for v in VARIANTS:
        s = sheet(v, tile)
        Ls = R.lum(s)
        painted = R.painted_mask(s, ground)
        gO = grad_mag(Ls)
        e = float(gO[edge].mean())
        efl = float(np.mean([gO[np.roll(np.roll(edge, dj, 0), di, 1)].mean()
                             for dj, di in DISPLACE]))
        sd = R3.signed_dist(painted)
        prof = R3.profile(Ls, sd)
        p2, at = R3.second_difference(prof)
        w = float(R3.dist_in(painted)[painted].mean()) if painted.any() else 0.0
        r = dict(variant=v, painted=int(painted.sum()),
                 terrain_mean=float(Ls[isTerrain].mean()),
                 terrain_minus_van=float((Ls - Lv)[isTerrain].mean()),
                 edge=e, edge_floor=efl, prof2=float(p2), prof2_at=at,
                 width=w,
                 road_mean=float(Ls[road0].mean()),
                 road_minus_van=float((Ls - Lv)[road0].mean()))
        rows.append(r)
        r['surf_mean'] = float(Ls[road0 & ~isTerrain].mean())
        r['two_tone'] = r['surf_mean'] - r['terrain_mean']
        print('   %-12s %8d %8.2f %+8.2f %8.3f %8.3f %8.3f %8.2f %8.2f %8.2f'
              % (v, r['painted'], r['terrain_mean'], r['terrain_minus_van'],
                 e, efl, p2, w, r['surf_mean'], r['two_tone']))
    # the two references on the same axis
    sdp = R3.signed_dist(proj)
    pv = R3.profile(Lv, R3.signed_dist(R.painted_mask(sheet('r4_default', tile),
                                                      ground)))
    p2v, atv = R3.second_difference(pv)
    print('   %-12s %8d %8.2f %+8.2f %8.3f %8.3f %8.3f %8.2f %8.2f %8.2f'
          % ('VANILLA', int(road0.sum()), float(Lv[isTerrain].mean()), 0.0,
             float(gV[edge].mean()),
             float(np.mean([gV[np.roll(np.roll(edge, dj, 0), di, 1)].mean()
                            for dj, di in DISPLACE])),
             float(p2v),
             float(R3.dist_in(proj)[proj].mean()),
             float(Lv[road0 & ~isTerrain].mean()),
             float(Lv[road0 & ~isTerrain].mean()) - float(Lv[isTerrain].mean())))
    print('   %-12s %8d %8.2f %+8.2f %8.3f %8s %8s %8.2f %8.2f %8.2f'
          % ('our ground', 0, float(Lg[isTerrain].mean()),
             float((Lg - Lv)[isTerrain].mean()),
             float(grad_mag(Lg)[edge].mean()), '-', '-',
             float(R3.dist_in(proj)[proj].mean()),
             float(Lg[road0 & ~isTerrain].mean()),
             float(Lg[road0 & ~isTerrain].mean()) - float(Lg[isTerrain].mean())))
    print('   (P VANILLA is read on the default bake\'s own distance axis, so '
          'the two P columns are the same texels)')
    print('   (W for VANILLA and for our ground is the PROJECTED road width, a '
          'world fact from the ESM: %.2f)' % float(R3.dist_in(proj)[proj].mean()))

    OUT[tile] = dict(rows=rows, vanilla=dict(
        terrain_mean=float(Lv[isTerrain].mean()), edge=float(gV[edge].mean()),
        prof2=float(p2v), width=float(R3.dist_in(proj)[proj].mean())),
        ground=dict(terrain_mean=float(Lg[isTerrain].mean())),
        sets=dict(painted=int(road0.sum()), terrain=int(isTerrain.sum()),
                  edge=int(edge.sum()), proj=int(proj.sum())))


def main():
    for t in ('t2020', 't0808'):
        run(t)
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    json.dump(OUT, open(os.path.join(HERE, 'logs', 'gp.json'), 'w'), indent=1)
    print('')
    print('wrote logs/gp.json')


if __name__ == '__main__':
    main()
