"""Corpus survey: is one tile's spacing the whole worldspace's spacing?

Reads EVERY Commonwealth .BTR at a level and reports the distribution of
distinct xy columns, triangles, the share of columns that sit on the 128-unit
LAND grid, and the modal vertex gap.  A single tile is an anecdote; this is
the population it came from.
"""
import glob
import os
import sys
import numpy as np
from btrparse import find_shapes, read_shape

D = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth'


def one(path, level):
    data = open(path, 'rb').read()
    sh = find_shapes(data)
    if not sh:
        return None
    pos, tri = read_shape(data, sh[0])
    p = pos * float(level)
    cols = np.unique(p[:, :2], axis=0)
    r = np.abs(cols / 128.0 - np.round(cols / 128.0)).max(axis=1)
    ux, uy = np.unique(p[:, 0]), np.unique(p[:, 1])
    g = np.concatenate([np.diff(ux), np.diff(uy)])
    gmode = float(np.bincount(np.round(g).astype(int)).argmax()) if len(g) else 0.0
    return dict(stored=sh[0]['numVerts'], tris=sh[0]['numTris'], cols=len(cols),
                ongrid=int((r < 1e-9).sum()), gmin=float(g.min()) if len(g) else 0,
                gmode=gmode, nshapes=len(sh))


def pct(a, q):
    return float(np.percentile(a, q))


for level in (4, 8, 16, 32):
    files = sorted(glob.glob(f'{D}/Commonwealth.{level}.*.BTR'))
    rows = []
    for f in files:
        try:
            r = one(f, level)
        except Exception as e:                       # a chunk we cannot parse is reported, never skipped silently
            print(f'  PARSE FAIL {os.path.basename(f)}: {e}')
            continue
        if r:
            r['f'] = os.path.basename(f)
            rows.append(r)
    cols = np.array([r['cols'] for r in rows])
    tris = np.array([r['tris'] for r in rows])
    og = np.array([100.0 * r['ongrid'] / r['cols'] for r in rows])
    gm = np.array([r['gmode'] for r in rows])
    gmin = np.array([r['gmin'] for r in rows])
    ncell = level * level
    print(f'LEVEL {level}: {len(rows)} of {len(files)} chunks parsed, {ncell} cells each')
    print(f'  distinct columns   min {cols.min()}  p25 {pct(cols,25):.0f}  median {np.median(cols):.0f}'
          f'  p75 {pct(cols,75):.0f}  p95 {pct(cols,95):.0f}  max {cols.max()}')
    print(f'  triangles          min {tris.min()}  median {np.median(tris):.0f}  p95 {pct(tris,95):.0f}  max {tris.max()}')
    print(f'  columns per cell   median {np.median(cols)/ncell:.3f}   -> spacing '
          f'{4096.0/np.sqrt(np.median(cols)/ncell):.0f} units/sample per axis')
    print(f'  on 128 grid (%)    min {og.min():.1f}  median {np.median(og):.1f}  mean {og.mean():.1f}')
    print(f'  chunks 100% on grid {int((og > 99.999).sum())} of {len(rows)}'
          f'   chunks below 90%: {int((og < 90).sum())}')
    print(f'  modal gap          {sorted(set(gm.tolist()))[:6]}   min gap seen {gmin.min()}')
    worst = sorted(rows, key=lambda r: r['ongrid'] / r['cols'])[:5]
    print('  least grid-aligned: ' + ', '.join(
        f"{r['f'].split('Commonwealth.')[1]} {100.0*r['ongrid']/r['cols']:.0f}%" for r in worst))
    big = sorted(rows, key=lambda r: -r['cols'])[:5]
    print('  densest chunks:     ' + ', '.join(
        f"{r['f'].split('Commonwealth.')[1]} {r['cols']}c/{r['tris']}t" for r in big))
    print()
