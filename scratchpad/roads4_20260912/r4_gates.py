"""ROADS4 -- the pre-registered gates, re-read in one place so every number in
the report has a file behind it.  Writes logs/gates.json.

G0/G5 byte identity is done in the shell (diff -rq, bake.log excluded because
it records the command line and the wall clock); this script does G1 (the
vertex-alpha correlation), G2 (the cross-road second difference) and G4 (the
painted mask width against the projected road width).
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(SCRATCH, 'roads3_20260911'))
import r4lib as R                                              # noqa: E402
import r3lib as R3                                             # noqa: E402

OUT = {}


def sheet(variant, tile):
    t = R.TILES[tile]
    import splatlib as S
    return S.Dds(os.path.join(HERE, 'out', variant, tile, 'tex',
                              'Commonwealth.4.%d.%d.DDS'
                              % (t['cx'], t['cy']))).level(0)[:, :, :3]         .astype(np.float64)


def corr(a, b):
    a = np.asarray(a, dtype=np.float64).ravel()
    b = np.asarray(b, dtype=np.float64).ravel()
    if a.size < 2 or a.std() < 1e-9 or b.std() < 1e-9:
        return float('nan')
    return float(np.corrcoef(a, b)[0, 1])


for tile in ('t2020', 't0808'):
    ground = sheet('r4_noroads', tile)
    van = R.vanilla_sheet(tile)
    d1 = sheet('r4_default', tile)
    pr = R.project(tile)
    shp, alp = pr['shp'], pr['alp']
    proj = shp >= 0
    mask = R.painted_mask(d1, ground) & proj
    Lo, Lv = R.lum(d1), R.lum(van)

    g1 = dict(
        texels=int(mask.sum()),
        alpha_mean=float(alp[mask].mean()),
        alpha_min=float(alp[mask].min()),
        alpha_below_099=int((alp[mask] < 0.99).sum()),
        corr_ours=corr(Lo[mask], alp[mask]),
        corr_vanilla=corr(Lv[mask], alp[mask]),
        corr_ground=corr(R.lum(ground)[mask], alp[mask]),
    )

    sd = R3.signed_dist(mask)
    p_ours, _ = R3.second_difference(R3.profile(Lo, sd))
    p_van, _ = R3.second_difference(R3.profile(Lv, sd))
    g2 = dict(ours=float(p_ours), vanilla=float(p_van))

    g4 = dict(painted_width=float(R3.dist_in(mask)[mask].mean()),
              projected_width=float(R3.dist_in(proj)[proj].mean()),
              painted_texels=int(mask.sum()), projected_texels=int(proj.sum()))

    OUT[tile] = dict(G1=g1, G2=g2, G4=g4)
    print('==', tile)
    for k in ('G1', 'G2', 'G4'):
        print('  ', k, json.dumps(OUT[tile][k]))

os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
json.dump(OUT, open(os.path.join(HERE, 'logs', 'gates.json'), 'w'), indent=1)
print('wrote logs/gates.json')
