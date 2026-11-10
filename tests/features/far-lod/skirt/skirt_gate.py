"""lane PERFLOAD1 P9: the far rings' skirt, offline.

1. Our rings (btdterrain.cpp buildTerrainSurface): a python twin of the wall rule on rings with a cut, a keep
   rectangle and several tiles. Every open edge of the drawn ground (an edge one drawn quad uses) must carry exactly one
   wall, and no wall may stand inside the ground.
2. The game rings (cellfarvanilla.cpp): the skirt mode per shape kind -- water is skirted like terrain unless the red is
   set, objects never.
3. The source still holds the rules the twin copies.

usage: python -I skirt_gate.py [--red]   (--red: the twin drops the walls on cut edges and water is unskirted; it fails)
"""
import os
import sys

RED = "--red" in sys.argv
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
fails = 0
checks = 0


def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)


def ring(cellX0, cellY0, cellsX, cellsY, n, k, cut, keep):
    """the drawn quads and the walls of one ring, as buildTerrainSurface makes them (grid vertex = (gx, gy))"""
    def inCut(cx, cy):
        return cut is not None and cut[0] <= cx <= cut[2] and cut[1] <= cy <= cut[3]
    wx0, wy0, wx1, wy1 = keep if keep else (cellX0, cellY0, cellX0 + cellsX - 1, cellY0 + cellsY - 1)

    def open_(cx, cy):
        return cx < wx0 or cx > wx1 or cy < wy0 or cy > wy1 or inCut(cx, cy)

    quads, walls = set(), []
    tilesX, tilesY = (cellsX + k - 1) // k, (cellsY + k - 1) // k
    for ty in range(tilesY):
        hT = min(k, cellsY - ty * k)
        for tx in range(tilesX):
            wT = min(k, cellsX - tx * k)
            ox = oy = 0
            wC, hC = wT, hT
            if keep:
                a0, b0 = cellX0 + tx * k, cellY0 + ty * k
                kx0, kx1 = max(a0, keep[0]), min(a0 + wT - 1, keep[2])
                ky0, ky1 = max(b0, keep[1]), min(b0 + hT - 1, keep[3])
                if kx1 < kx0 or ky1 < ky0:
                    continue
                ox, oy, wC, hC = kx0 - a0, ky0 - b0, kx1 - kx0 + 1, ky1 - ky0 + 1
            tileX, tileY = tx * k + ox, ty * k + oy
            gx0, gy0 = tileX * n, tileY * n
            wV, hV = wC * n + 1, hC * n + 1
            for j in range(hV - 1):
                for i in range(wV - 1):
                    cx, cy = cellX0 + tileX + i // n, cellY0 + tileY + j // n
                    if inCut(cx, cy):
                        continue
                    quads.add((gx0 + i, gy0 + j))
                    g = lambda ii, jj: (gx0 + ii, gy0 + jj)
                    # the red twin leaves the cut edges open (the old rule had no walls at all)
                    if i % n == 0 and open_(cx - 1, cy) and not (RED and inCut(cx - 1, cy)):
                        walls.append(frozenset((g(i, j), g(i, j + 1))))
                    if (i + 1) % n == 0 and open_(cx + 1, cy) and not (RED and inCut(cx + 1, cy)):
                        walls.append(frozenset((g(i + 1, j), g(i + 1, j + 1))))
                    if j % n == 0 and open_(cx, cy - 1) and not (RED and inCut(cx, cy - 1)):
                        walls.append(frozenset((g(i, j), g(i + 1, j))))
                    if (j + 1) % n == 0 and open_(cx, cy + 1) and not (RED and inCut(cx, cy + 1)):
                        walls.append(frozenset((g(i, j + 1), g(i + 1, j + 1))))
    return quads, walls


def open_edges(quads):
    uses = {}
    for (x, y) in quads:
        for e in (((x, y), (x + 1, y)), ((x + 1, y), (x + 1, y + 1)), ((x, y + 1), (x + 1, y + 1)), ((x, y), (x, y + 1))):
            f = frozenset(e)
            uses[f] = uses.get(f, 0) + 1
    return {e for e, c in uses.items() if c == 1}


CASES = [
    # name, cellX0, cellY0, cellsX, cellsY, n, k, cut, keep
    ("ring 0 exact, cut block", -6, -6, 12, 12, 4, 3, (-2, -2, 2, 2), (-5, -5, 5, 5)),
    ("ring 1, cut = ring 0", -12, -12, 24, 24, 2, 5, (-5, -5, 5, 5), None),
    ("ring, cut on the tile line", 0, 0, 8, 8, 1, 4, (0, 0, 3, 3), None),
    ("ring, odd cells, one-cell cut", 3, -4, 7, 5, 8, 2, (5, -2, 5, -2), None),
    ("ring, no cut", 0, 0, 5, 5, 4, 2, None, None),
]
for name, cx0, cy0, cxs, cys, n, k, cut, keep in CASES:
    quads, walls = ring(cx0, cy0, cxs, cys, n, k, cut, keep)
    want = open_edges(quads)
    got = set(walls)
    check(len(walls) == len(got), "%s: a doubled wall (%d walls, %d edges)" % (name, len(walls), len(got)))
    check(got == want, "%s: %d open edges, %d walled, %d left open, %d inside the ground"
          % (name, len(want), len(got), len(want - got), len(got - want)))
    print("ring %-30s quads %5d  open edges %4d  walls %4d  skirt tris %5d" % (name, len(quads), len(want), len(got), 4 * len(got)))


def skirt_mode(kind, water, outermost, red):
    """cellfarvanilla.cpp's call site: 0 none, 1 the cut and ours lines, 2 also the ring's rim"""
    return 0 if kind != 0 or (water and red) else (1 if outermost else 2)


for kind in (0, 1):
    for water in (False, True):
        for outer in (False, True):
            m = skirt_mode(kind, water, outer, RED)
            if kind != 0:
                check(m == 0, "objects skirted (kind %d)" % kind)
            else:
                check(m == (1 if outer else 2), "terrain kind 0 water %s outermost %s: mode %d" % (water, outer, m))

# 3. the source holds what the twin copies
def src(p):
    with open(os.path.join(ROOT, p), "rb") as f:
        return f.read().decode("utf-8", "replace")


van = src("src/features/far-lod/cellfarvanilla.cpp")
btd = src("src/features/far-ground/btdterrain.cpp")
lod = src("src/features/far-lod/cellfarlod.cpp")
for needle, text, what in (
        ("kind != 0 || ( water[size_t( k )] && skirtRed ) ? 0 : ( &r == &rings.back() ? 1 : 2 )", van, "vanilla skirt mode"),
        ('wwEnvInt( "WW_CELL_FARLOD_SKIRT_RED" ) == 1', van, "vanilla red"),
        ("ring.skirtCapped++;", van, "vanilla cap count"),
        ("ring.clipCapped++;", van, "vanilla clip cap count"),
        ("if ( i % n == 0 && open( cx - 1, cy ) )", btd, "ours west wall"),
        ("if ( ( i + 1 ) % n == 0 && open( cx + 1, cy ) )", btd, "ours east wall"),
        ("if ( j % n == 0 && open( cx, cy - 1 ) )", btd, "ours south wall"),
        ("if ( ( j + 1 ) % n == 0 && open( cx, cy + 1 ) )", btd, "ours north wall"),
        ("return cx < wx0 || cx > wx1 || cy < wy0 || cy > wy1 || inCut( cx, cy );", btd, "ours open rule"),
        ("footSrc[size_t( i - wV * hV )]", btd, "ours foot colour"),
        ("water skirt tris", lod, "far-lod line: water"),
        ("skirt tris (%2 edges over the 16-bit cap)", lod, "far-lod line: ours"),
):
    check(needle in text, "source: " + what)

print("skirt gate: %d of %d checks passed%s" % (checks - fails, checks, " (RED twin)" if RED else ""))
sys.exit(1 if fails else 0)
