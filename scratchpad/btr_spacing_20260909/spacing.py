"""Terrain-LOD geometry resolution: measured, not recalled.

For each .BTR the land shape's vertices are converted to WORLD units (local
half-float position x the shape's world scale, which is the level) and then:

  * unique (x,y) columns, because a vanilla chunk is a near-unindexed triangle
    soup -- 11713 stored vertices at level 4 are only 1982 distinct columns;
  * the sorted unique world x and y and the gaps between them, which is what
    "vertex spacing" actually means (the task's instruction);
  * how many columns sit exactly on the 128-unit LAND grid;
  * triangle edge lengths, the honest resolution of an adaptive mesh;
  * a 4x4 tile breakdown, to see whether the spacing is uniform over the chunk.
"""
import sys
import numpy as np
from collections import Counter
from btrparse import find_shapes, read_shape

CELL = 4096.0
LAND = 128.0


def analyse(path, level, label):
    data = open(path, 'rb').read()
    shapes = find_shapes(data)
    if not shapes:
        print(f"{label}: NO SHAPES FOUND")
        return None
    s = shapes[0]                      # shape 0 is 'Land'; later ones are WATER
    pos, tri = read_shape(data, s)
    p = pos * float(level)             # miniature space -> world units
    cols = np.unique(p[:, :2], axis=0)
    ux = np.unique(p[:, 0])
    uy = np.unique(p[:, 1])
    span = ux[-1] - ux[0]
    gx = np.diff(ux)
    gy = np.diff(uy)
    g = np.concatenate([gx, gy])
    # grid membership, exact (half floats represent every multiple of
    # 128/level exactly at these magnitudes, so 0 tolerance is legitimate)
    ongrid = np.sum((np.abs(cols[:, 0] / LAND - np.round(cols[:, 0] / LAND)) < 1e-9)
                    & (np.abs(cols[:, 1] / LAND - np.round(cols[:, 1] / LAND)) < 1e-9))
    # triangle edges
    a, b, c = p[tri[:, 0]], p[tri[:, 1]], p[tri[:, 2]]
    e = np.concatenate([np.linalg.norm(a[:, :2] - b[:, :2], axis=1),
                        np.linalg.norm(b[:, :2] - c[:, :2], axis=1),
                        np.linalg.norm(c[:, :2] - a[:, :2], axis=1)])
    e = e[e > 1e-6]
    ncell = level * level
    full_grid = (level * 32 + 1) ** 2
    eff = span / (np.sqrt(len(cols)) - 1.0)
    print(f"=== {label}  level {level}  ({level}x{level} = {ncell} cells)")
    print(f"  stored vertices        {s['numVerts']}   triangles {s['numTris']}"
          f"   vertexSize {s['vsize']}  desc 0x{s['desc']:012X}")
    print(f"  distinct xy columns    {len(cols)}")
    print(f"  world extent           {ux[0]:.0f} .. {ux[-1]:.0f} x  "
          f"{uy[0]:.0f} .. {uy[-1]:.0f} y   span {span:.0f} "
          f"(= {span/CELL:.0f} cells)")
    print(f"  unique x / unique y    {len(ux)} / {len(uy)}")
    print(f"  gap min / median / max {g.min():.4g} / {np.median(g):.4g} / {g.max():.4g}")
    top = Counter(np.round(g, 4)).most_common(6)
    print(f"  most common gaps       {top}")
    print(f"  columns on 128 grid    {ongrid} of {len(cols)} "
          f"({100.0*ongrid/len(cols):.1f}%)")
    print(f"  full LAND grid here    {level*32+1}^2 = {full_grid} columns "
          f"-> vanilla keeps {100.0*len(cols)/full_grid:.2f}%")
    print(f"  triangle edge len      min {e.min():.4g}  median {np.median(e):.4g}"
          f"  mean {e.mean():.4g}  p90 {np.percentile(e,90):.4g}  max {e.max():.4g}")
    print(f"  effective spacing      span/(sqrt(cols)-1) = {eff:.1f} units")
    print(f"    -> samples per cell  {CELL/eff:.2f} per axis    "
          f"vs LAND {CELL/LAND:.0f}   ratio {eff/LAND:.2f}x coarser")
    print(f"  triangles per cell     {s['numTris']/ncell:.1f}")
    # uniformity: 4x4 tiles over the chunk, columns per tile
    n = 4
    tw = span / n
    counts = np.zeros((n, n), dtype=int)
    for iy in range(n):
        for ix in range(n):
            m = ((cols[:, 0] >= ux[0] + ix*tw) & (cols[:, 0] <= ux[0] + (ix+1)*tw) &
                 (cols[:, 1] >= uy[0] + iy*tw) & (cols[:, 1] <= uy[0] + (iy+1)*tw))
            counts[iy, ix] = m.sum()
    print(f"  columns per quarter-tile (4x4, y up):")
    for row in counts[::-1]:
        print("     " + " ".join(f"{v:6d}" for v in row))
    print(f"    tile min/max ratio   {counts.max()/max(counts.min(),1):.1f}x")
    # border / skirt: how many columns lie exactly on the chunk edge
    edge = np.sum((cols[:, 0] == ux[0]) | (cols[:, 0] == ux[-1]) |
                  (cols[:, 1] == uy[0]) | (cols[:, 1] == uy[-1]))
    print(f"  columns on chunk edge  {edge}")
    print()
    return dict(level=level, cells=ncell, stored=s['numVerts'], tris=s['numTris'],
                cols=len(cols), span=span, gmin=g.min(), gmed=float(np.median(g)),
                gmax=g.max(), ongrid=int(ongrid), eff=eff,
                emed=float(np.median(e)), emin=e.min(), emax=e.max())


if __name__ == '__main__':
    for a in sys.argv[1:]:
        path, _, rest = a.partition('|')
        level, _, label = rest.partition('|')
        analyse(path, int(level), label or path)
