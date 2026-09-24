"""A2d, third and exact form.

The first two forms measured the distance from a texel to the nearest occupied
lattice square and compared it against a hand-derived bound.  Both were proxies,
and both reported darkening a little farther out than the bound allowed, which
is a fact about the proxy and not about the code.

This form asks the question the code actually answers: the march reads exactly
56 points -- 8 directions x 7 distances 128,192,288,432,648,972,1458 -- and if
none of those 56 points lands in an occupied square the object term returns
1.0f by early return and the byte CANNOT move.  So:

  * every darkened texel must have at least one occupied sample (or be BC1
    block spill from a neighbour that does);
  * the largest occupied-sample distance over the whole region IS the reach,
    measured rather than asserted.
"""
import os, sys, struct, math
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/ground1_20260912/work')
import lodgen_vt_check as V
import maskdec

NONE = -1.0e29
DIRS = [(1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0),
        (0.7071, 0.7071), (0.7071, -0.7071),
        (-0.7071, 0.7071), (-0.7071, -0.7071)]
DISTS = []
_d = 128.0
while _d <= 2048.0:
    DISTS.append(_d)
    _d *= 1.5


def read_field(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'OBJH', b[:4]
    gx0, gy0, gw, gh = struct.unpack_from('<4i', b, 4)
    cell, = struct.unpack_from('<f', b, 20)
    g = struct.unpack_from('<%df' % (gw * gh), b, 24)
    return gx0, gy0, gw, gh, cell, g


def mask_rows(v, index, mip=0):
    """The mask sheet's decoded rows.  Routed through maskdec because a COVER
    tile's mask sheet is BC3, 16 bytes a block with the alpha half first, and
    `lodgen_vt_check.decode_bc1` walks 8-byte blocks -- which silently returns
    garbage for every cover tile.  This lane read tiles 4..15 that way once."""
    rows, fmt = maskdec.mask_rows(v, index, mip)
    return rows


def geometry(path):
    v = V.Lodv(path)
    dim = v.levelDim
    upt = dim * 4096.0 / v.content
    out = {}
    for ty in range(v.tilesY):
        for tx in range(v.tilesX):
            i = ty * v.tilesX + tx
            cellX0 = v.west + tx * dim
            cellY0 = v.north - (ty + 1) * dim + 1
            out[i] = (cellX0 * 4096.0, (cellY0 + dim) * 4096.0, upt)
    return v, out


def main(off_path, on_path, field_path):
    gx0, gy0, gw, gh, cell, g = read_field(field_path)
    occ = bytearray(1 if g[k] > NONE else 0 for k in range(gw * gh))
    print('field %dx%d cell %g origin (%d,%d) occupied %d'
          % (gw, gh, cell, gx0, gy0, sum(occ)))
    print('march: %d directions x %d distances %s'
          % (len(DIRS), len(DISTS), ','.join('%g' % d for d in DISTS)))

    vo, geo = geometry(off_path)
    vn, _ = geometry(on_path)
    b, n = vo.border, vo.stored

    stat = {'dark_hit': 0, 'dark_miss': 0, 'flat_hit': 0, 'flat_miss': 0,
            'bright_hit': 0, 'bright_miss': 0}
    far_dark = 0.0
    hist = {}
    misses = []
    spill_in_block, spill_alone, spill_drops = 0, 0, []
    alone_list = []
    for i in sorted(geo):
        ro, rn = mask_rows(vo, i), mask_rows(vn, i)
        if ro is None or rn is None:
            continue
        tW, tN, upt = geo[i]
        hit = [[False] * n for _ in range(n)]
        for y in range(b, n - b):
            wy = tN - (y - b + 0.5) * upt
            for x in range(b, n - b):
                wx = tW + (x - b + 0.5) * upt
                far = -1.0
                for (dx, dy) in DIRS:
                    for d in DISTS:
                        gx = int(math.floor((wx + dx * d) / cell)) - gx0
                        gy = int(math.floor((wy + dy * d) / cell)) - gy0
                        if 0 <= gx < gw and 0 <= gy < gh and occ[gy * gw + gx]:
                            if d > far:
                                far = d
                a, o = ro[y][x][2], rn[y][x][2]
                hit[y][x] = far > 0
                key = 'dark' if o < a else ('bright' if o > a else 'flat')
                stat[key + ('_hit' if far > 0 else '_miss')] += 1
                if key == 'dark':
                    if far > far_dark:
                        far_dark = far
                    if far > 0:
                        hist[far] = hist.get(far, 0) + 1
                    elif len(misses) < 8:
                        misses.append((a - o, wx, wy, i, x, y))
        # BC1 is a 4x4 block codec: a block whose endpoints are refitted moves
        # every texel in it, including ones the term itself never touched.  So
        # every darkened texel with no occluder of its own must SHARE ITS BLOCK
        # with one that has an occluder, or the claim is wrong.
        for y in range(b, n - b):
            for x in range(b, n - b):
                a, o = ro[y][x][2], rn[y][x][2]
                if o >= a or hit[y][x]:
                    continue
                by, bx = (y >> 2) << 2, (x >> 2) << 2
                near = any(hit[by + q][bx + r] for q in range(4) for r in range(4))
                spill_drops.append(a - o)
                if near:
                    spill_in_block += 1
                else:
                    spill_alone += 1
                    alone_list.append((a - o, i, x, y))

    tot = sum(stat.values())
    print('content texels %d' % tot)
    print('                    at least one occupied sample    none')
    for key in ('dark', 'flat', 'bright'):
        print('  %-7s %14d %25d' % (key, stat[key + '_hit'], stat[key + '_miss']))
    print()
    print('  darkened texels whose 56 samples are ALL empty: %d'
          % stat['dark_miss'])
    print('    (the object term returns 1.0f by early return there, so any such')
    print('     texel is BC1 block spill, never the term itself)')
    if misses:
        print('    the first few:')
        for (dr, wx, wy, i, x, y) in misses:
            print('      drop %3d world (%9.0f,%9.0f) tile %2d texel (%3d,%3d)'
                  % (dr, wx, wy, i, x, y))
    if spill_drops:
        spill_drops.sort()
        print('    of those %d: %d share their 4x4 BC1 block with a texel that DOES'
              % (len(spill_drops), spill_in_block))
        print('    have an occluder, %d do not; drops median %d, p95 %d, max %d'
              % (spill_alone, spill_drops[len(spill_drops) // 2],
                 spill_drops[len(spill_drops) * 19 // 20], spill_drops[-1]))
        if alone_list:
            ys, xs, ts = {}, {}, {}
            for (dr, i, x, y) in alone_list:
                ys[y] = ys.get(y, 0) + 1
                xs[x] = xs.get(x, 0) + 1
                ts[i] = ts.get(i, 0) + 1
            print('    the %d in no such block, by texel ROW y (top 12):' % len(alone_list))
            for k in sorted(ys, key=lambda z: -ys[z])[:12]:
                print('      y %3d  %6d' % (k, ys[k]))
            print('    by texel COLUMN x (top 12):')
            for k in sorted(xs, key=lambda z: -xs[z])[:12]:
                print('      x %3d  %6d' % (k, xs[k]))
            print('    by tile:')
            for k in sorted(ts):
                print('      tile %2d  %6d' % (k, ts[k]))
    print()
    print('  A2d, the reach MEASURED: the largest march distance at which a')
    print('  darkened texel found an occluder is %.1f u; the loop\'s longest'
          % far_dark)
    print('  step is 1458.0 u.')
    print('  darkened texels by their farthest occupied sample:')
    for k in sorted(hist):
        print('    %7.1f u  %8d' % (k, hist[k]))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
