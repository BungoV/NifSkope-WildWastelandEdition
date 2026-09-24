#!/usr/bin/env python
"""THE SILHOUETTE GATE, measured independently of the writer (lane NATIVE1c).

bungo, 2026-09-11 16:1x, over native1b's `ladder.png`: *"Hm, that tree LOD
becomes a stump there"*.  The rule the writer now enforces is that every level
of every mesh keeps at least a stated fraction of level 0's outline, seen from
the horizon.  This script re-derives that number from the `.lodo` BYTES, with
its own rasteriser, so the gate is not the writer grading its own homework.

It follows `ww-silhouette-compare` and `ww-control-calibration`:

  * KNOWN ANSWERS first.  The metric is run on inputs whose answer is known
    before any real number is printed: a soup against ITSELF must read 1.0, and
    a soup against an empty one must read 0.0.  A metric that cannot fail on
    its input is not a metric.
  * THE CEILING is the unsimplified mesh -- the same data with the property
    (simplification) removed.  It must read 1.0.
  * THE FLOOR is an independent twin built in the domain of the number it
    floors: the level-0 soup with RANDOM VERTICES DROPPED until it carries the
    SAME TRIANGLE COUNT as the level being judged.  Matched on the quantity the
    metric reads -- how much geometry is gone -- not on a proxy.  It must score
    BELOW the floor the writer enforces, or the gate is passing on a measure
    that cannot go red.

The cut a level draws is reconstructed from the file the way the runtime would:
a cluster is in the level-`lv` cut when its own level is at or below `lv` and
its parent group's first cluster is either absent (a root) or at a level above
`lv`.  That is the same set the writer measured -- the retired roots, the
clusters no group at this level consumed, and the new simplified ones.

    python tests/spells/lodgen_silhouette.py <ws.lodo> [--floor 0.70]
        [--meshes 40] [--views 8] [--grid 96] [--seed 7]

Prints `N checks, M failures` then `RESULT PASS`/`FAIL`; exit 1 on any failure.
"""
import argparse
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from lodgen_native_cut import (  # noqa: E402
    read_lodo, cluster_positions, cluster_triangles, NO_PARENT)


class Checks(object):
    def __init__(self):
        self.n = 0
        self.fails = 0

    def check(self, name, ok, extra=''):
        self.n += 1
        if ok:
            print('  ok   %s%s' % (name, (' ' + str(extra)) if extra != '' else ''))
        else:
            self.fails += 1
            print('  FAIL %s%s' % (name, (' ' + str(extra)) if extra != '' else ''))
        return ok


# ------------------------------------------------------------ the rasteriser
def coverage(tris, pos, lo, ex, views, grid):
    """One coverage count per horizon view, over the mesh's OWN box.

    The view is an azimuth about Z at the horizon: u along the view's right,
    v along world Z.  Both soups are rasterised over the SAME box, so the two
    pictures are the same picture and the ratio is about the geometry.
    """
    cx = lo[0] + ex[0] * 0.5
    cy = lo[1] + ex[1] * 0.5
    hx = ex[0] * 0.5
    hy = ex[1] * 0.5
    rad = math.sqrt(hx * hx + hy * hy)
    if rad <= 0.0 or ex[2] <= 0.0:
        return [0] * views
    out = []
    for vi in range(views):
        a = 2.0 * math.pi * vi / views
        ca, sa = math.cos(a), math.sin(a)
        grid_set = set()
        for (ia, ib, ic) in tris:
            p = []
            for idx in (ia, ib, ic):
                x, y, z = pos[idx]
                u = (x - cx) * ca + (y - cy) * sa
                gu = (u + rad) / (2.0 * rad) * grid
                gv = (z - lo[2]) / ex[2] * grid
                p.append((gu, gv))
            mark_triangle(grid_set, p, grid)
        out.append(len(grid_set))
    return out


def mark_triangle(acc, p, grid):
    (x0, y0), (x1, y1), (x2, y2) = p
    minx = max(0, int(math.floor(min(x0, x1, x2))))
    maxx = min(grid - 1, int(math.ceil(max(x0, x1, x2))))
    miny = max(0, int(math.floor(min(y0, y1, y2))))
    maxy = min(grid - 1, int(math.ceil(max(y0, y1, y2))))
    if minx > maxx or miny > maxy:
        return
    d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    if abs(d) < 1e-12:
        # EDGE-ON OR DEGENERATE. A leaf quad seen edge-on has no area in this
        # view but it is still there: mark its own segments, or the metric
        # rewards a simplifier for deleting exactly the faces that vanish here.
        for (ax, ay), (bx, by) in ((p[0], p[1]), (p[1], p[2]), (p[2], p[0])):
            steps = int(max(abs(bx - ax), abs(by - ay))) + 1
            for s in range(steps + 1):
                t = s / float(steps)
                gx = int(ax + (bx - ax) * t)
                gy = int(ay + (by - ay) * t)
                if 0 <= gx < grid and 0 <= gy < grid:
                    acc.add(gy * grid + gx)
        return
    for gy in range(miny, maxy + 1):
        py = gy + 0.5
        for gx in range(minx, maxx + 1):
            px = gx + 0.5
            w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / d
            w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / d
            w2 = 1.0 - w0 - w1
            if w0 >= 0.0 and w1 >= 0.0 and w2 >= 0.0:
                acc.add(gy * grid + gx)


def ratio(cut, full, pos, lo, ex, views, grid):
    """The WORST view's kept fraction. Worst, not mean: a tree that holds its
    outline from seven sides and is a stump from the eighth is still a stump."""
    a = coverage(cut, pos, lo, ex, views, grid)
    b = coverage(full, pos, lo, ex, views, grid)
    worst = 1.0
    seen = False
    for i in range(views):
        if b[i] <= 0:
            continue
        seen = True
        worst = min(worst, a[i] / float(b[i]))
    return worst if seen else 1.0


# ------------------------------------------------------------------- the cuts
def mesh_tables(L, mi):
    """Every cluster of the mesh as (level, parentFirst, triangles), with one
    shared position list so a cut is a list of index triples."""
    m = L['meshes'][mi]
    pos = []
    per = []
    for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        base = len(pos)
        pos.extend(cluster_positions(L, ci))
        tris = [(base + a, base + b, base + c) for (a, b, c) in cluster_triangles(L, ci)]
        per.append((ci, L['lods'][ci]['level'], L['lods'][ci]['parentFirst'], tris))
    return pos, per


def cut_at_level(L, per, lv):
    """The soup a viewer sees when the mesh is drawn at level `lv`."""
    out = []
    for (ci, level, pfirst, tris) in per:
        if level > lv:
            continue
        if pfirst != NO_PARENT and L['lods'][pfirst]['level'] <= lv:
            continue
        out.extend(tris)
    return out


def drop_twin(tris, want, rng):
    """THE FLOOR. Drop random VERTICES from the full soup -- with every triangle
    that touches them -- until the soup is down to `want` triangles. Dropping
    vertices rather than triangles is what makes it a twin of a simplifier: both
    remove vertices, one of them cares where.

    One vertex at a time, with a vertex -> triangle index so it stays cheap, and
    it stops on the CONSERVATIVE side: the last vertex that would take the twin
    BELOW `want` is put back, so the twin always carries at least as many
    triangles as the level it floors and is never handed an advantage.
    """
    of_vert = {}
    for ti, t in enumerate(tris):
        for v in t:
            of_vert.setdefault(v, []).append(ti)
    verts = sorted(of_vert)
    rng.shuffle(verts)
    alive = [True] * len(tris)
    live = len(tris)
    for v in verts:
        if live <= want:
            break
        gone = [ti for ti in of_vert[v] if alive[ti]]
        if not gone:
            continue
        if live - len(gone) < want:
            continue            # this one would overshoot; leave it in place
        for ti in gone:
            alive[ti] = False
        live -= len(gone)
    return [t for ti, t in enumerate(tris) if alive[ti]]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('--floor', type=float, default=0.70)
    ap.add_argument('--meshes', type=int, default=24)
    ap.add_argument('--views', type=int, default=8)
    ap.add_argument('--grid', type=int, default=96)
    ap.add_argument('--seed', type=int, default=7)
    ap.add_argument('--tri-budget', type=int, default=60000, dest='tri_budget')
    ap.add_argument('--max-mesh-tris', type=int, default=3000,
                    dest='max_mesh_tris')
    a = ap.parse_args()
    L = read_lodo(a.lodo)
    ck = Checks()
    rng = random.Random(a.seed)

    # ---- 1. known answers, before any real number -------------------------
    pos = [(0.0, 0.0, 0.0), (10.0, 0.0, 0.0), (0.0, 0.0, 10.0), (0.0, 10.0, 5.0)]
    tri = [(0, 1, 2), (0, 2, 3)]
    lo, ex = (0.0, 0.0, 0.0), (10.0, 10.0, 10.0)
    ck.check('K1 the metric reads 1.0 on a soup against ITSELF',
             abs(ratio(tri, tri, pos, lo, ex, a.views, a.grid) - 1.0) < 1e-9)
    ck.check('K2 the metric reads 0.0 on an EMPTY soup against that soup',
             ratio([], tri, pos, lo, ex, a.views, a.grid) == 0.0)

    # ---- 2. the real meshes ----------------------------------------------
    laddered = [mi for mi, m in enumerate(L['meshes']) if m['levelCount'] > 1]

    def mesh_tris(mi):
        m = L['meshes'][mi]
        return sum(L['clusters'][ci]['triangleCount']
                   for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']))

    # THE LARGEST LADDERS FIRST, under a TRIANGLE BUDGET. This rasteriser is
    # pure Python and a near-model library's meshes are the real models', so
    # "the 40 largest" is minutes a mesh, not seconds. The budget bounds the
    # run and the coverage it bought is PRINTED, so a shrinking sample is
    # visible rather than silent.
    sized = sorted(((mesh_tris(mi), mi) for mi in laddered), reverse=True)
    picked = []
    spent = 0
    skipped = 0
    for (t, mi) in sized:
        if t > a.max_mesh_tris:
            skipped += 1
            continue
        if len(picked) >= a.meshes or spent + t > a.tri_budget:
            continue
        picked.append(mi)
        spent += t
    print('%d meshes carry a ladder, the largest %d triangles; %d are above the '
          '%d-triangle per-mesh cap and are skipped; measuring %d meshes, %d '
          'level-0 triangles (budget %d)'
          % (len(laddered), sized[0][0] if sized else 0, skipped, a.max_mesh_tris,
             len(picked), spent, a.tri_budget))

    worstKept = 1.0
    worstWhere = ''
    levelsSeen = 0
    ceilings = []
    floors = []
    for mi in picked:
        m = L['meshes'][mi]
        lo = (m['ax'], m['ay'], m['az'])
        ex = (m['ex'], m['ey'], m['ez'])
        pos, per = mesh_tables(L, mi)
        full = cut_at_level(L, per, 0)
        if not full:
            continue
        ceilings.append(ratio(full, full, pos, lo, ex, a.views, a.grid))
        top = max(p[1] for p in per)
        for lv in range(1, top + 1):
            cut = cut_at_level(L, per, lv)
            if not cut:
                continue
            levelsSeen += 1
            r = ratio(cut, full, pos, lo, ex, a.views, a.grid)
            if r < worstKept:
                worstKept = r
                worstWhere = '%s level %d' % (L['string_at'](m['modelStringOffset']), lv)
            if lv == top:
                twin = drop_twin(full, len(cut), rng)
                floors.append((ratio(twin, full, pos, lo, ex, a.views, a.grid),
                               len(twin), len(cut)))

    ck.check('S1 the CEILING reads 1.0 on every mesh (the unsimplified soup against '
             'itself, %d meshes)' % len(ceilings),
             bool(ceilings) and all(abs(c - 1.0) < 1e-9 for c in ceilings),
             'worst %.4f' % (min(ceilings) if ceilings else 0.0))
    ck.check('S2 every kept level holds the floor %.2f (%d levels over %d meshes)'
             % (a.floor, levelsSeen, len(picked)),
             levelsSeen > 0 and worstKept >= a.floor,
             'worst %.4f at %s' % (worstKept, worstWhere))
    fr = [f[0] for f in floors]
    below = [f for f in fr if f < a.floor]
    ck.check('S3 FLOOR the random-vertex-drop twin, matched on triangle count, FAILS the '
             'same gate on %d of %d meshes' % (len(below), len(fr)),
             bool(fr) and len(below) * 2 > len(fr),
             'twin worst %.4f, median %.4f' % (min(fr) if fr else 0.0,
                                               sorted(fr)[len(fr) // 2] if fr else 0.0))
    ck.check('S4 the twin is a fair twin: it is never SMALLER than the level it floors, '
             'and never more than a fifth larger',
             bool(floors) and all(c <= t <= max(c + 2, c * 1.2) for (_, t, c) in floors),
             '%d pairs, worst excess %.1f percent'
             % (len(floors), max([(t - c) * 100.0 / max(1, c) for (_, t, c) in floors] or [0.0])))
    print('worstKeptFraction %.4f' % worstKept)
    print('twinMedianFraction %.4f' % (sorted(fr)[len(fr) // 2] if fr else 0.0))
    print('%d checks, %d failures' % (ck.n, ck.fails))
    print('RESULT %s' % ('PASS' if not ck.fails else 'FAIL'))
    return 1 if ck.fails else 0


if __name__ == '__main__':
    sys.exit(main())
