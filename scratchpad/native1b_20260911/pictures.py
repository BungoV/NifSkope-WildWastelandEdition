#!/usr/bin/env python
"""The two pictures lane NATIVE1b owes, both drawn from the DECODER'S OWN
geometry -- never from a number the writer printed, and never a screenshot.

  images/ladder.png     one library mesh at levels 0, 1, 2 and 3 side by side,
                        wireframe, with its triangle count and its stored
                        geometric error burned into each panel.
  images/occluders.png  the occluder region's cells from above, every placement
                        a dot and every written box its footprint, so "a few
                        boxes per cell for buildings and hills" is visible as a
                        map rather than as a count.

    python pictures.py <sanctuary .lodo> <sanctuary .lodi> <occ .lodo> <occ .lodi> <outdir>

Pillow only (matplotlib is not installed on this machine).
"""
import math
import os
import struct
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', '..', 'tests', 'spells'))
from lodgen_native_cut import read_lodo, read_lodi, cluster_positions, cluster_triangles  # noqa: E402

INK = (24, 42, 68)
MUTED = (150, 150, 150)
GRID = (222, 222, 222)
BOX = (217, 119, 6)
BOXEDGE = (124, 45, 18)
DOT = (120, 155, 190)
BG = (255, 255, 255)


def most_placed_mesh(L, T, wantLevels=4):
    """The mesh the region places MOST OFTEN that has at least `wantLevels`
    levels -- chosen from the data, so the picture is of something the region
    actually draws and not of whatever looked good."""
    count = {}
    for r in T['instances']:
        if 'x' not in r:
            continue
        b = L['bases'][r['baseId']]
        for k in range(4):
            if b['rep%d' % k] != 0xFFFF:
                count[b['rep%d' % k]] = count.get(b['rep%d' % k], 0) + 1
                break
    ranked = sorted(count.items(), key=lambda kv: -kv[1])
    for mi, n in ranked:
        if L['meshes'][mi]['levelCount'] >= wantLevels:
            return mi, n
    return ranked[0] if ranked else (0, 0)


def level_geometry(L, mi, lv):
    m = L['meshes'][mi]
    segs = []
    tris = 0
    err = 0.0
    for ci in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        cl = L['lods'][ci]
        if cl['level'] != lv:
            continue
        err = max(err, cl['err'])
        pos = cluster_positions(L, ci)
        for (a, b, c) in cluster_triangles(L, ci):
            tris += 1
            for u, v in ((a, b), (b, c), (c, a)):
                segs.append((pos[u], pos[v]))
    return segs, tris, err


def draw_ladder(L, T, out):
    mi, placements = most_placed_mesh(L, T)
    m = L['meshes'][mi]
    name = os.path.basename(L['string_at'](m['modelStringOffset']).replace('\\', '/'))
    diag = math.sqrt(m['ex'] ** 2 + m['ey'] ** 2 + m['ez'] ** 2)
    levels = list(range(min(4, m['levelCount'])))
    PW, PH = 420, 420
    TOP, CAP = 54, 62
    W = PW * len(levels)
    H = TOP + PH + CAP
    img = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(img)
    d.text((14, 8), '%s   --   .lodo mesh %d, %d placements in the nine-chunk Sanctuary region, '
                    '%.0f-unit diagonal' % (name, mi, placements, diag), fill=INK)
    d.text((10, 24), 'drawn from the .lodo\'s own cluster geometry: front elevation, world X across, '
                     'world Z up, every triangle of every cluster at that level', fill=INK)
    d.text((10, 40), 'the ladder is per (mesh, material) and a level is built only where a group could '
                     'be simplified, so a level may cover only part of the mesh', fill=MUTED)
    l0 = None
    for pi, lv in enumerate(levels):
        segs, tris, err = level_geometry(L, mi, lv)
        if lv == 0:
            l0 = tris
        x0 = pi * PW
        d.rectangle([x0 + 6, TOP, x0 + PW - 6, TOP + PH], outline=GRID)
        sx = (PW - 40) / m['ex'] if m['ex'] > 0 else 1.0
        sy = (PH - 40) / m['ez'] if m['ez'] > 0 else 1.0
        s = min(sx, sy)
        ox = x0 + PW / 2 - (m['ax'] + m['ex'] / 2) * s
        oy = TOP + PH / 2 + (m['az'] + m['ez'] / 2) * s
        for (p, q) in segs:
            d.line([ox + p[0] * s, oy - p[2] * s, ox + q[0] * s, oy - q[2] * s], fill=INK)
        cy = TOP + PH + 8
        d.text((x0 + 12, cy), 'level %d' % lv, fill=INK)
        d.text((x0 + 12, cy + 14), '%d triangles  (%.0f%% of level 0)'
               % (tris, 100.0 * tris / l0 if l0 else 0.0), fill=INK)
        d.text((x0 + 12, cy + 28), 'geometricError %.1f u' % err, fill=INK)
        d.text((x0 + 12, cy + 42), '= %.1f%% of the model diagonal' % (100.0 * err / diag if diag else 0.0),
               fill=MUTED)
    img.save(out)
    return name, mi, placements, l0, [level_geometry(L, mi, lv)[1] for lv in levels]


def draw_occluders(L, T, occRanges, out):
    h = T['header']
    inst = [r for r in T['instances'] if 'x' in r]
    # crop to the INSTANCES, padded one cell: the dense chunk table spans a
    # wider box than the bake filled, and an empty margin is not information
    xs0 = min(r['x'] for r in inst) - 4096.0
    xs1 = max(r['x'] for r in inst) + 4096.0
    ys0 = min(r['y'] for r in inst) - 4096.0
    ys1 = max(r['y'] for r in inst) + 4096.0
    SIZE = 980
    TOP = 56
    img = Image.new('RGB', (SIZE, SIZE + TOP + 26), BG)
    d = ImageDraw.Draw(img)
    s = min((SIZE - 20) / (xs1 - xs0), (SIZE - 20) / (ys1 - ys0))

    def P(wx, wy):
        return (10 + (wx - xs0) * s, TOP + SIZE - 10 - (wy - ys0) * s)

    x = math.floor(xs0 / 4096.0) * 4096.0
    while x <= xs1:
        d.line([P(x, ys0), P(x, ys1)], fill=GRID)
        x += 4096.0
    y = math.floor(ys0 / 4096.0) * 4096.0
    while y <= ys1:
        d.line([P(xs0, y), P(xs1, y)], fill=GRID)
        y += 4096.0
    for r in inst:
        px, py = P(r['x'], r['y'])
        d.point((px, py), fill=DOT)
    for o in T['occluders']:
        r = T['instances'][o['instanceIndex']]
        mm = r['m']
        pts = []
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            lx, ly = sx * o['hx'], sy * o['hy']
            wx = o['x'] + mm[0] * lx + mm[1] * ly
            wy = o['y'] + mm[3] * lx + mm[4] * ly
            pts.append(P(wx, wy))
        d.polygon(pts, fill=BOX, outline=BOXEDGE)
        # a box a few hundred units across is two pixels at this scale, so its
        # footprint gets a ring that keeps it findable without inflating it
        cxp = sum(p[0] for p in pts) / 4.0
        cyp = sum(p[1] for p in pts) / 4.0
        d.ellipse([cxp - 5, cyp - 5, cxp + 5, cyp + 5], outline=BOXEDGE)
    cells = sum(1 for r in occRanges if r[1])
    pop = 0
    d.text((10, 8), 'Precomputed occluders, downtown region (cells 0,-12 .. 11,-1), seen from above',
           fill=INK)
    d.text((10, 24), '%d boxes over %d placements; every box is a footprint of an oriented box fitted '
                     'INSIDE a watertight LOD mesh' % (len(T['occluders']), len(inst)), fill=INK)
    d.text((10, 40), 'the light grid is the 4,096-unit cell the boxes are binned into, at most four a cell; '
                     '%d cells carry one' % cells, fill=MUTED)
    d.text((10, TOP + SIZE + 6), 'blue dots = .lodi instance positions;  orange = occluder box footprint '
                                 '(the box is a volume; this is its plan)', fill=MUTED)
    img.save(out)
    return len(T['occluders']), len(inst), cells, pop


def main():
    slodo, slodi, olodo, olodi, outdir = sys.argv[1:6]
    os.makedirs(outdir, exist_ok=True)
    L = read_lodo(slodo)
    T = read_lodi(slodi)
    name, mi, pl, l0, counts = draw_ladder(L, T, os.path.join(outdir, 'ladder.png'))
    print('ladder.png: %s (mesh %d, %d placements, triangles per level %s)' % (name, mi, pl, counts))
    L2 = read_lodo(olodo)
    T2 = read_lodi(olodi)
    b = open(olodi, 'rb').read()
    offR = struct.unpack_from('<Q', b, 0xA0)[0]
    occRanges = [struct.unpack_from('<II', b, offR + i * 8)
                 for i in range(T2['header']['presentChunks'] * 16)]
    n, inst, cells, _ = draw_occluders(L2, T2, occRanges, os.path.join(outdir, 'occluders.png'))
    print('occluders.png: %d boxes over %d placements, %d cells with a box' % (n, inst, cells))


main()
