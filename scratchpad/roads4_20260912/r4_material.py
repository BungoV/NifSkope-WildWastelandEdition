"""ROADS4 item 1b -- WHAT the road pass actually paints, by the winning
shape's MATERIAL, and the vertex-alpha correlation ROADS3 reported, re-measured
on an instrument whose unreached texels are not silently zero.

Item 1's first run answered the brief's question and refused its premise: the
vertex-alpha SKIRT paints ZERO texels that a trunk triangle does not also
cover, on both chunks.  So "exclude the skirt triangles" cannot be the fix, and
the terrain bungo is seeing in the bake is not the vertex-alpha feather.

This run asks the next question instead: the road NIFs carry shapes whose
MATERIAL is not a road surface at all -- `dirtgravel01`, `forestfloor01alpha`
-- modelled as the verge beside the asphalt.  Those are opaque, they get
coverage 1, and the bake prints them as road paint.  Measured here:

  * every material that wins a texel, with its texel count;
  * the split ROAD SURFACE vs TERRAIN-LIKE, by a name rule stated below;
  * mean luminance of each class in our detail-1 bake, our own `--no-roads`
    ground and Bethesda's shipped sheet;
  * the luminance GRADIENT at the boundary of the terrain-material patches,
    against the same boundary set displaced five ways (the floor), which is
    what "a sharp mesh edge in the bake" means as a number.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r4lib as R                                              # noqa: E402

DISPLACE = [(37, 0), (0, 37), (-29, 23), (23, -29), (53, 53)]

# THE FOLDER RULE, stated before any number is read, and data-driven rather
# than guessed from a stem.  Bethesda files every landscape material under a
# folder that names what it is:
#
#   materials/Landscape/Ground/*   the same ground materials the LANDSCAPE
#                                  itself is painted with -- terrain
#   materials/Landscape/Roads/*    the road surface, its kerbs, its decals
#
# so the discriminator is the material's own FOLDER, not its name.  The first
# run of this file used a stem list and mis-classed two of the biggest winners
# on chunk (-20,20) -- `CommonwealthDefault01.bgsm` (7,558 texels) and
# `SancSW01.BGSM` (5,317) -- as unclassed; `CommonwealthDefault01` sits under
# Ground and is the Commonwealth's default terrain material.  Material paths
# in the NIFs carry four different prefixes (`materials\...`,
# `Data\materials\...`, `C:\Projects\Fallout4\Build\PC\Data\Materials\...`
# and lowercase variants), so the rule matches on the SUFFIX `landscape/<x>/`
# after case folding and separator folding, never on the whole string.
GROUND_FOLDER = 'landscape/ground/'
ROAD_FOLDERS = ('landscape/roads/', 'landscape/sidewalks/')

OUT = {}


def classify(mat):
    p = (mat or '').replace(chr(92), '/').lower()
    if not p:
        return 'no-material'
    if GROUND_FOLDER in p:
        return 'terrain'
    for f in ROAD_FOLDERS:
        if f in p:
            return 'road'
    return 'other-landscape'


def grad_mag(L):
    gy, gx = np.gradient(L)
    return np.hypot(gx, gy)


def boundary_of(mask, road):
    """Texels of `mask` with a 4-neighbour inside `road` but outside `mask` --
    the patch's own edge WITHIN the painted road, which is where a mesh-shaped
    terrain patch meets the asphalt."""
    edge = np.zeros_like(mask)
    for dj, di in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        nb = np.roll(np.roll(mask, dj, axis=0), di, axis=1)
        edge |= mask & ~nb & np.roll(np.roll(road, dj, axis=0), di, axis=1)
    return edge


def run(tile):
    print('')
    print('=' * 78)
    print('CHUNK %s' % tile)
    print('=' * 78)
    ours = R.sheet('rung_detail1', tile)
    ground = R.sheet('rung_noroads', tile)
    van = R.vanilla_sheet(tile)
    Lo, Lg, Lv = (R.lum(x) for x in (ours, ground, van))

    pr = R.project(tile)
    shp = pr['shp']
    painted = R.painted_mask(ours, ground)
    road = painted & (shp >= 0)

    shapes = pr['shapes']
    cls_of_shape = np.array([classify(s['mat']) == 'terrain' for s in shapes])
    unc_of_shape = np.array([classify(s['mat']) in ('other-landscape',
                                                    'no-material')
                             for s in shapes])
    idx = np.clip(shp, 0, None)
    isTerrain = road & cls_of_shape[idx]
    isUnc = road & unc_of_shape[idx]
    isRoadSurf = road & ~isTerrain & ~isUnc

    # ---------------------------------------------------- the material table
    names, keyOf = [], {}
    matOfShape = np.zeros(len(shapes), dtype=np.int32)
    for sid, s in enumerate(shapes):
        b = os.path.basename((s['mat'] or '(none)').replace(chr(92), '/'))
        k = b.lower()
        if k not in keyOf:
            keyOf[k] = len(names)
            names.append(dict(name=b, texels=0, cls=classify(s['mat'])))
        matOfShape[sid] = keyOf[k]
    matbuf = matOfShape[idx]
    rows = []
    print('')
    print('WHAT WINS A ROAD TEXEL, by material (%d classified texels)'
          % int(road.sum()))
    print('   %-44s %8s %10s %9s %9s %9s'
          % ('material', 'texels', 'class', 'ours', 'ground', 'vanilla'))
    for mi, m in enumerate(names):
        sel = road & (matbuf == mi)
        n = int(sel.sum())
        if n == 0:
            continue
        m['texels'] = n
        m['ours'] = float(Lo[sel].mean())
        m['ground'] = float(Lg[sel].mean())
        m['vanilla'] = float(Lv[sel].mean())
        rows.append(m)
    rows.sort(key=lambda m: -m['texels'])
    for m in rows:
        print('   %-44s %8d %10s %9.2f %9.2f %9.2f'
              % (m['name'][:44], m['texels'], m['cls'], m['ours'],
                 m['ground'], m['vanilla']))

    # ------------------------------------------------------------ the split
    print('')
    print('THE SPLIT')
    tbl = []
    for name, m in (('road surface', isRoadSurf), ('TERRAIN-LIKE', isTerrain),
                    ('other/no material', isUnc), ('all classified road', road),
                    ('off-road ground (control)', ~painted & (shp < 0))):
        r = dict(set=name, texels=int(m.sum()))
        if m.any():
            r['ours'] = float(Lo[m].mean())
            r['ground'] = float(Lg[m].mean())
            r['vanilla'] = float(Lv[m].mean())
            r['ours_minus_ground'] = r['ours'] - r['ground']
            r['ours_minus_vanilla'] = r['ours'] - r['vanilla']
            r['van_minus_ground'] = r['vanilla'] - r['ground']
        tbl.append(r)
        print('   %-28s %8d  ours %7.2f  ground %7.2f  vanilla %7.2f   '
              'ours-ground %+7.2f  ours-van %+7.2f  van-ground %+7.2f'
              % (name, r['texels'], r.get('ours', float('nan')),
                 r.get('ground', float('nan')), r.get('vanilla', float('nan')),
                 r.get('ours_minus_ground', float('nan')),
                 r.get('ours_minus_vanilla', float('nan')),
                 r.get('van_minus_ground', float('nan'))))

    # ------------------------------------------- the edge of a terrain patch
    print('')
    print('THE EDGE: mean luminance gradient at the terrain patch boundary')
    edge = boundary_of(isTerrain, road)
    gO, gG, gV = grad_mag(Lo), grad_mag(Lg), grad_mag(Lv)
    def g(m):
        return (float(gO[m].mean()), float(gG[m].mean()), float(gV[m].mean())) \
            if m.any() else (float('nan'),) * 3
    eo, eg, ev = g(edge)
    print('   terrain-patch boundary   %6d texels   ours %6.3f  our ground '
          '%6.3f  vanilla %6.3f' % (int(edge.sum()), eo, eg, ev))
    fl = []
    for dj, di in DISPLACE:
        m = np.roll(np.roll(edge, dj, axis=0), di, axis=1)
        a, b, c = g(m)
        fl.append((a, b, c))
        print('   floor displaced %+4d%+4d           ours %6.3f  our ground '
              '%6.3f  vanilla %6.3f' % (dj, di, a, b, c))
    if fl:
        print('   floor mean               ours %6.3f  our ground %6.3f  '
              'vanilla %6.3f' % tuple(np.mean(fl, axis=0)))

    # --------------------------- the vertex-alpha correlation, re-instrumented
    print('')
    print('THE VERTEX-ALPHA CORRELATION (ROADS3 F3e\', re-measured)')
    a = pr['alp'][road]
    if a.std() > 0:
        for lbl, F in (('ours(d1)', Lo), ('our ground', Lg), ('vanilla', Lv)):
            c = float(np.corrcoef(a, F[road])[0, 1])
            print('   corr(luminance, winning vertex alpha)  %-12s %+0.4f'
                  % (lbl, c))
        print('   alpha field on the road: min %.3f  mean %.4f  sd %.4f  '
              '(texels below 1: %d)'
              % (a.min(), a.mean(), a.std(), int((a < 1.0).sum())))
    else:
        print('   the alpha field is constant on the road mask; no correlation')

    OUT[tile] = dict(materials=rows, split=tbl,
                     edge=dict(texels=int(edge.sum()), ours=eo, ground=eg,
                               vanilla=ev, floor=fl))


def main():
    for tile in ('t2020', 't0808'):
        run(tile)
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    json.dump(OUT, open(os.path.join(HERE, 'logs', 'material.json'), 'w'),
              indent=1, default=float)
    print('')
    print('wrote logs/material.json')


if __name__ == '__main__':
    main()
