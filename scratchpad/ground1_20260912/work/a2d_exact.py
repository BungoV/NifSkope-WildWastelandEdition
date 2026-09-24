"""A2d re-measured with an EXACT euclidean distance transform (Felzenszwalb),
because the 3-4 chamfer used in the first pass is only an approximation and the
first pass reported darkening farther out than the march can reach.
"""
import os, sys, struct, math
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_vt_check as V

NONE = -1.0e29
REACH = 1458.0
INF = float('inf')
BIG = 1.0e12


def read_field(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'OBJH', b[:4]
    gx0, gy0, gw, gh = struct.unpack_from('<4i', b, 4)
    cell, = struct.unpack_from('<f', b, 20)
    g = struct.unpack_from('<%df' % (gw * gh), b, 24)
    return gx0, gy0, gw, gh, cell, g


def edt1d(f):
    """Felzenszwalb 1-D squared EDT. Uses a large FINITE sentinel rather than
    float inf, because inf - inf produced a NaN in the first attempt."""
    n = len(f)
    d = [0.0] * n
    v = [0] * n
    z = [0.0] * (n + 1)
    k = 0
    v[0] = 0
    z[0] = -BIG
    z[1] = BIG
    for q in range(1, n):
        s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0 * q - 2.0 * v[k])
        while s <= z[k] and k > 0:
            k -= 1
            s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2.0 * q - 2.0 * v[k])
        k += 1
        v[k] = q
        z[k] = s
        z[k + 1] = BIG
    k = 0
    for q in range(n):
        while z[k + 1] < q:
            k += 1
        d[q] = (q - v[k]) ** 2 + f[v[k]]
    return d


def edt2d(occ, gw, gh):
    """Exact squared euclidean distance, in squared LATTICE units."""
    f = [0.0 if occ[k] else BIG for k in range(gw * gh)]
    for x in range(gw):
        col = edt1d([f[y * gw + x] for y in range(gh)])
        for y in range(gh):
            f[y * gw + x] = col[y]
    for y in range(gh):
        row = edt1d(f[y * gw:(y + 1) * gw])
        f[y * gw:(y + 1) * gw] = row
    return f


def mask_rows(v, index, mip=0):
    e = v.table[index]
    p = v.payload(index)
    if p is None:
        return None
    ms = [s for s in range(v.sheetCount) if v.sheets[s]['role'] == 5][0]
    o = v.sheetOffset(bool(e['flags'] & 2), ms, mip)
    side = v.stored >> mip
    return V.decode_bc1(p, o, side, side)


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
    occ = [g[k] > NONE for k in range(gw * gh)]
    print('field %dx%d cell %g origin (%d,%d) occupied %d'
          % (gw, gh, cell, gx0, gy0, sum(1 for x in occ if x)))
    sq = edt2d(occ, gw, gh)

    vo, geo = geometry(off_path)
    vn, _ = geometry(on_path)
    b, n = vo.border, vo.stored
    texel = geo[0][2]

    bands, beyond = {}, []
    limit = REACH + cell * math.sqrt(2.0) + texel
    for i in sorted(geo):
        ro, rn = mask_rows(vo, i), mask_rows(vn, i)
        if ro is None or rn is None:
            continue
        tW, tN, upt = geo[i]
        for y in range(b, n - b):
            wy = tN - (y - b + 0.5) * upt
            for x in range(b, n - b):
                wx = tW + (x - b + 0.5) * upt
                a, o = ro[y][x][2], rn[y][x][2]
                gx = int(math.floor(wx / cell)) - gx0
                gy = int(math.floor(wy / cell)) - gy0
                if gx < 0 or gy < 0 or gx >= gw or gy >= gh:
                    continue
                d = math.sqrt(sq[gy * gw + gx]) * cell
                e = bands.setdefault(int(d // 256) * 256, [0, 0, 0])
                e[1] += 1
                if o > a:
                    e[2] += 1
                elif o < a:
                    e[0] += 1
                    if d > limit:
                        beyond.append((d, a - o, wx, wy, i, x, y))
    print('limit = reach %g + lattice diagonal %.1f + texel %g = %.1f u'
          % (REACH, cell * math.sqrt(2.0), texel, limit))
    print('  band          darkened /   total      %%   BRIGHTER')
    for k in sorted(bands):
        print('  %5d..%5d  %8d / %8d  %5.1f%%  %6d'
              % (k, k + 256, bands[k][0], bands[k][1],
                 100.0 * bands[k][0] / max(1, bands[k][1]), bands[k][2]))
    beyond.sort(key=lambda t: -t[1])
    print('  darkened beyond the limit: %d' % len(beyond))
    for t in (0, 2, 4, 8, 16):
        print('    by more than %2d bytes: %d' % (t, sum(1 for z in beyond if z[1] > t)))
    print('  the twelve biggest drops beyond the limit:')
    for (d, dr, wx, wy, i, x, y) in beyond[:12]:
        print('    d %7.1f u  drop %3d  world (%9.0f,%9.0f)  tile %2d texel (%3d,%3d)'
              % (d, dr, wx, wy, i, x, y))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
