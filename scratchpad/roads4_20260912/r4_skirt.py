"""ROADS4 item 1 -- the SIZE OF THE DEFECT, measured before any rule is picked.

  python r4_skirt.py            # both tiles, table to stdout + logs/skirt.json

Every number is read off bakes that already exist on disk
(`scratchpad/roads3_20260911/out/...`, made by the built exe of 2026-09-12
04:10:38) and off Bethesda's own shipped sheets.  Nothing here runs the
generator and nothing here is simulated.

THE INSTRUMENT IS CHECKED BEFORE IT IS READ (ww-control-calibration):

  C1  the projected road mask against the mask the GENERATOR actually painted
      (`|ours - ours --no-roads| >= 1`).  These are two independent routes to
      the same world fact -- ours through a re-typed python rasteriser over the
      ESM's placements, the generator's through its own C++ -- so their overlap
      is the instrument's accuracy and their disagreement is reported, never
      hidden.  Texels the generator painted that the projection does not reach
      are counted as UNCLASSIFIED and excluded from every class statistic.
  C2  a displaced-mask floor for every luminance statement: the same mask
      translated five ways, area and shape preserved, registration with the
      road destroyed.  A class difference that the floor also shows is not the
      road's.
"""
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r4lib as R                                              # noqa: E402

DISPLACE = [(37, 0), (0, 37), (-29, 23), (23, -29), (53, 53)]
OUT = {}


def stats(name, mask, fields):
    row = {'set': name, 'texels': int(mask.sum())}
    for k, f in fields.items():
        row[k] = float(f[mask].mean()) if mask.any() else float('nan')
    return row


def run(tile):
    print('')
    print('=' * 78)
    print('CHUNK %s  (cells %d..%d x %d..%d)'
          % (tile, R.TILES[tile]['cx'], R.TILES[tile]['cx'] + 3,
             R.TILES[tile]['cy'], R.TILES[tile]['cy'] + 3))
    print('=' * 78)

    ours = R.sheet('rung_detail1', tile)      # the NEW default: detail 1
    old = R.sheet('rung_roads', tile)         # today's default: detail 0
    ground = R.sheet('rung_noroads', tile)
    van = R.vanilla_sheet(tile)
    Lo, Lold, Lg, Lv = (R.lum(x) for x in (ours, old, ground, van))

    painted = R.painted_mask(ours, ground)
    paintedOld = R.painted_mask(old, ground)
    pr = R.project(tile)
    proj = pr['shp'] >= 0
    anyT = pr['trunk'] > 0
    anyS = pr['skirt'] > 0

    # ---------------------------------------------------------------- C1
    inter = painted & proj
    onlyGen = painted & ~proj
    onlyProj = proj & ~painted
    print('')
    print('C1 the instrument: projected mask vs the mask the generator painted')
    print('   generator painted        %7d texels (detail 1 bake)' % painted.sum())
    print('   generator painted        %7d texels (detail 0 bake, for scale)'
          % paintedOld.sum())
    print('   python projection        %7d' % proj.sum())
    print('   both                     %7d  = %.1f%% of the generator\'s'
          % (inter.sum(), 100.0 * inter.sum() / max(1, painted.sum())))
    print('   generator only           %7d  -> UNCLASSIFIED, dropped below'
          % onlyGen.sum())
    print('   projection only          %7d  (alpha-tested cut-outs the '
          'projection does not sample, and the hasLod arm)' % onlyProj.sum())

    # -------------------------------------------------------- the triangles
    t = pr['tris']
    print('')
    print('THE TRIANGLES of the flat-road family projected onto this chunk')
    print('   trunk (all three vertex alphas 1) %8d' % t['trunk'])
    print('   skirt (any vertex alpha below 1)  %8d  = %.1f%%'
          % (t['skirt'], 100.0 * t['skirt'] / max(1, t['trunk'] + t['skirt'])))
    print('   shapes %d, of which carrying any skirt triangle %d'
          % (len(pr['shapes']),
             sum(1 for s in pr['shapes'] if s['skirtTris'] > 0)))
    print('')
    print('   %-52s %7s %7s %6s' % ('material', 'trunk', 'skirt', 'skirt%'))
    rows = sorted(t['byMaterial'].items(), key=lambda kv: -kv[1]['skirt'])
    for k, v in rows[:12]:
        tot = v['trunk'] + v['skirt']
        print('   %-52s %7d %7d %5.1f%%'
              % (os.path.basename(k.replace(chr(92), '/'))[:52], v['trunk'],
                 v['skirt'], 100.0 * v['skirt'] / max(1, tot)))
    nz = sum(1 for _, v in rows if v['skirt'] > 0)
    print('   %d of %d materials carry skirt triangles' % (nz, len(rows)))

    # ------------------------------------------------------------ the texels
    road = inter
    trunkOnly = road & anyT & ~anyS
    both = road & anyT & anyS
    skirtOnly = road & anyS & ~anyT
    neither = road & ~anyT & ~anyS
    winSkirt = road & (pr['cls'] == 1)

    fields = {'ours(d1)': Lo, 'ours(d0)': Lold, 'ground': Lg, 'vanilla': Lv}
    rowsT = [stats('all road texels', road, fields),
             stats('TRUNK only (no skirt triangle covers)', trunkOnly, fields),
             stats('both trunk and skirt cover', both, fields),
             stats('SKIRT ONLY -- the defect', skirtOnly, fields),
             stats('neither (bookkeeping, must be 0)', neither, fields),
             stats('max-z winner IS a skirt triangle', winSkirt, fields),
             stats('off-road ground (control)', ~painted & ~proj, fields)]
    print('')
    print('THE TEXELS, mean luminance')
    print('   %-40s %8s %9s %9s %9s %9s'
          % ('set', 'texels', 'ours(d1)', 'ours(d0)', 'ground', 'vanilla'))
    for r in rowsT:
        print('   %-40s %8d %9.2f %9.2f %9.2f %9.2f'
              % (r['set'], r['texels'], r['ours(d1)'], r['ours(d0)'],
                 r['ground'], r['vanilla']))

    print('')
    print('   SKIRT-ONLY, the three differences that name the defect')
    if skirtOnly.any():
        d_g = float((Lo - Lg)[skirtOnly].mean())
        d_v = float((Lo - Lv)[skirtOnly].mean())
        v_g = float((Lv - Lg)[skirtOnly].mean())
        print('     ours - our own ground under them  %+8.2f levels' % d_g)
        print('     ours - vanilla at the same texels %+8.2f levels' % d_v)
        print('     vanilla - our ground (the same texels, for scale) %+6.2f'
              % v_g)
        print('   C2 floors, the same mask displaced (ours - ground):')
        fl = []
        for dj, di in DISPLACE:
            m = np.roll(np.roll(skirtOnly, dj, axis=0), di, axis=1)
            fl.append(float((Lo - Lg)[m].mean()))
            print('     %+4d%+4d  %+8.2f' % (dj, di, fl[-1]))
        print('     floor mean %+0.2f  worst |floor| %0.2f  against the '
              'measurement %+0.2f' % (np.mean(fl), max(abs(x) for x in fl), d_g))
    else:
        print('     no skirt-only texels on this chunk')

    # ------------------------------------------------------ the alpha profile
    print('')
    print('   SKIRT-ONLY texels binned by the winning triangle\'s vertex alpha')
    edges = [0.0, 0.1, 0.25, 0.4, 0.55, 0.7, 0.85, 1.0]
    print('     %-12s %8s %9s %9s %9s' % ('alpha', 'texels', 'ours(d1)',
                                          'ground', 'vanilla'))
    bins = []
    for k in range(len(edges) - 1):
        m = skirtOnly & (pr['alp'] >= edges[k]) & (pr['alp'] < edges[k + 1])
        b = dict(lo=edges[k], hi=edges[k + 1], texels=int(m.sum()))
        if m.any():
            b['ours'] = float(Lo[m].mean())
            b['ground'] = float(Lg[m].mean())
            b['vanilla'] = float(Lv[m].mean())
            print('     %.2f-%.2f  %8d %9.2f %9.2f %9.2f'
                  % (edges[k], edges[k + 1], m.sum(), b['ours'], b['ground'],
                     b['vanilla']))
        else:
            print('     %.2f-%.2f  %8d' % (edges[k], edges[k + 1], 0))
        bins.append(b)

    OUT[tile] = dict(
        c1=dict(painted=int(painted.sum()), paintedOld=int(paintedOld.sum()),
                proj=int(proj.sum()), both=int(inter.sum()),
                genOnly=int(onlyGen.sum()), projOnly=int(onlyProj.sum())),
        tris=t, rows=rowsT, bins=bins,
        skirtOnly=int(skirtOnly.sum()), trunkOnly=int(trunkOnly.sum()),
        bothCover=int(both.sum()), winSkirt=int(winSkirt.sum()))


def main():
    for tile in ('t2020', 't0808'):
        run(tile)
    os.makedirs(os.path.join(HERE, 'logs'), exist_ok=True)
    json.dump(OUT, open(os.path.join(HERE, 'logs', 'skirt.json'), 'w'),
              indent=1)
    print('')
    print('wrote logs/skirt.json')


if __name__ == '__main__':
    main()
