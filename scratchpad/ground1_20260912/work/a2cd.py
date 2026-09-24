"""Lane GROUND1 gates A2c (the footprint against five displaced controls) and
A2d (the reach), on the shipped pyramid.

The SUBJECT footprint and the reach both come from the object height field
dumped by the exe (--dump-object-ao), never from the darkening map, or the gates
would be circular.
"""
import os, sys, struct, math, random
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/ground1_20260912/work')
import lodgen_vt_check as V
import maskdec

NONE = -1.0e29
REACH = 1458.0


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


def distance_transform(occ, gw, gh):
    """Chebyshev-free two-pass chamfer in LATTICE units (3-4 chamfer scaled)."""
    INF = 10 ** 9
    d = [0 if occ[k] else INF for k in range(gw * gh)]
    for y in range(gh):
        for x in range(gw):
            k = y * gw + x
            if d[k] == 0:
                continue
            best = d[k]
            if x > 0:
                best = min(best, d[k - 1] + 3)
            if y > 0:
                best = min(best, d[k - gw] + 3)
                if x > 0:
                    best = min(best, d[k - gw - 1] + 4)
                if x + 1 < gw:
                    best = min(best, d[k - gw + 1] + 4)
            d[k] = best
    for y in range(gh - 1, -1, -1):
        for x in range(gw - 1, -1, -1):
            k = y * gw + x
            if d[k] == 0:
                continue
            best = d[k]
            if x + 1 < gw:
                best = min(best, d[k + 1] + 3)
            if y + 1 < gh:
                best = min(best, d[k + gw] + 3)
                if x + 1 < gw:
                    best = min(best, d[k + gw + 1] + 4)
                if x > 0:
                    best = min(best, d[k + gw - 1] + 4)
            d[k] = best
    return d   # in thirds of a lattice cell


def main(off_path, on_path, field_path):
    gx0, gy0, gw, gh, cell, g = read_field(field_path)
    occ = [g[k] > NONE for k in range(gw * gh)]
    nocc = sum(1 for x in occ if x)
    tops = sorted(g[k] for k in range(gw * gh) if occ[k])
    print('object field: %d x %d squares of %g u, origin (%d,%d), %d occupied'
          % (gw, gh, cell, gx0, gy0, nocc))
    print('  object tops: min %.1f  p05 %.1f  median %.1f  p95 %.1f  max %.1f'
          % (tops[0], tops[len(tops) // 20], tops[len(tops) // 2],
             tops[len(tops) * 19 // 20], tops[-1]))

    vo, geo = geometry(off_path)
    vn, _ = geometry(on_path)
    b, n = vo.border, vo.stored

    pts = []
    for i in sorted(geo):
        ro, rn = mask_rows(vo, i), mask_rows(vn, i)
        if ro is None or rn is None:
            continue
        tW, tN, upt = geo[i]
        for y in range(b, n - b):
            wy = tN - (y - b + 0.5) * upt
            for x in range(b, n - b):
                pts.append((tW + (x - b + 0.5) * upt, wy, ro[y][x][2], rn[y][x][2]))
    texel = geo[0][2]
    print('content texels compared: %d  (texel %g u)' % (len(pts), texel))

    bycell = {}
    for (wx, wy, a, o) in pts:
        bycell.setdefault((int(math.floor(wx / cell)), int(math.floor(wy / cell))),
                          []).append((a, o))

    # ------------------------------------------------ A2d: the reach --------
    print()
    print('== A2d: the reach, measured against the field, not against the map ==')
    dt = distance_transform(occ, gw, gh)
    bands = {}
    limit = REACH + cell * 1.5 + texel
    beyond = [0, 0, 0, 0]     # drop > 0, > 2, > 4, > 8
    within = 0
    for (wx, wy, a, o) in pts:
        gx = int(math.floor(wx / cell)) - gx0
        gy = int(math.floor(wy / cell)) - gy0
        if gx < 0 or gy < 0 or gx >= gw or gy >= gh:
            continue
        d = dt[gy * gw + gx] / 3.0 * cell
        band = int(d // 256) * 256
        e = bands.setdefault(band, [0, 0, 0])
        e[1] += 1
        if o > a:
            e[2] += 1            # BRIGHTER: impossible for the term, so it is
        if o < a:                # the block compressor's own noise
            e[0] += 1
            if d > limit:
                for t, th in enumerate((0, 2, 4, 8)):
                    if a - o > th:
                        beyond[t] += 1
            else:
                within += 1
    print('  distance to the nearest occupied square: darkened / total / BRIGHTENED')
    print('  (brightening is impossible for a term that only multiplies visibility')
    print('   downward, so the brightened column IS the BC1 block noise floor)')
    for k in sorted(bands):
        if k > 4352:
            continue
        print('    %5d..%5d u   %7d / %7d   %5.1f%%   %5d'
              % (k, k + 256, bands[k][0], bands[k][1],
                 100.0 * bands[k][0] / max(1, bands[k][1]), bands[k][2]))
    tail = sum(bands[k][1] for k in bands if k > 4352)
    tailm = sum(bands[k][0] for k in bands if k > 4352)
    tailb = sum(bands[k][2] for k in bands if k > 4352)
    print('    beyond 4352 u        %7d / %7d           %5d' % (tailm, tail, tailb))
    print('  A2d: reach + one lattice diagonal + one texel = %.1f u' % limit)
    print('       darkened beyond it: %d by any amount, %d by more than 2 bytes,'
          ' %d by more than 4, %d by more than 8'
          % (beyond[0], beyond[1], beyond[2], beyond[3]))
    print('       floor: %d texels nearer than that moved' % within)

    # ------------------------------------------------ A2c: the footprint ----
    print()
    print('== A2c: a building footprint against five displaced controls ==')
    occset = set(k for k in range(gw * gh) if occ[k])
    seen, blobs = set(), []
    for k in occset:
        if k in seen:
            continue
        stack, comp = [k], []
        seen.add(k)
        while stack:
            q = stack.pop(); comp.append(q)
            qx, qy = q % gw, q // gw
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = qx + dx, qy + dy
                if 0 <= nx < gw and 0 <= ny < gh:
                    nk = ny * gw + nx
                    if nk in occset and nk not in seen:
                        seen.add(nk); stack.append(nk)
        blobs.append(comp)
    sized = [c for c in blobs if 16 <= len(c) <= 200]
    sized.sort(key=lambda c: -len(c))
    subj = sized[0]
    sx = [k % gw + gx0 for k in subj]
    sy = [k // gw + gy0 for k in subj]
    cx = (sum(sx) / float(len(sx)) + 0.5) * cell
    cy = (sum(sy) / float(len(sy)) + 0.5) * cell
    print('  blobs %d; the subject is the largest of the %d between 16 and 200 squares:'
          % (len(blobs), len(sized)))
    print('    %d squares, top %.1f, centre world (%.0f, %.0f)'
          % (len(subj), max(g[k] for k in subj), cx, cy))
    fw = [(k % gw + gx0) * cell - cx for k in subj]
    fh = [(k // gw + gy0) * cell - cy for k in subj]

    def drop_over(ox, oy):
        want = set()
        for a, c in zip(fw, fh):
            want.add((int(math.floor((ox + a) / cell)), int(math.floor((oy + c) / cell))))
        tot, cnt = 0, 0
        for k in want:
            for (a, o) in bycell.get(k, ()):
                tot += a - o; cnt += 1
        return ((tot / float(cnt), cnt) if cnt else (None, 0))

    s_drop, s_n = drop_over(cx, cy)
    print('  SUBJECT   mean AO drop %.2f bytes over %d texels' % (s_drop, s_n))

    def footprint_clear(ox, oy):
        for a, c in zip(fw, fh):
            gx = int(math.floor((ox + a) / cell)) - gx0
            gy = int(math.floor((oy + c) / cell)) - gy0
            if gx < 0 or gy < 0 or gx >= gw or gy >= gh:
                return False
            if occ[gy * gw + gx]:
                return False
        return True

    allx = [p[0] for p in pts]; ally = [p[1] for p in pts]
    rcx = (min(allx) + max(allx)) / 2.0; rcy = (min(ally) + max(ally)) / 2.0
    rad = math.hypot(cx - rcx, cy - rcy)
    a0 = math.atan2(cy - rcy, cx - rcx)
    rnd = random.Random(20260912)
    ctrl, tried = [], 0
    while len(ctrl) < 5 and tried < 20000:
        tried += 1
        ang = a0 + rnd.uniform(0.4, 2 * math.pi - 0.4)
        ox, oy = rcx + rad * math.cos(ang), rcy + rad * math.sin(ang)
        if not footprint_clear(ox, oy):
            continue
        d, c = drop_over(ox, oy)
        if d is None or c < s_n // 2:
            continue
        ctrl.append((ox, oy, d, c))
    for k, (ox, oy, d, c) in enumerate(ctrl):
        print('  control %d at (%7.0f,%7.0f)  mean AO drop %7.2f over %d texels'
              % (k + 1, ox, oy, d, c))
    if ctrl:
        worst = max(c[2] for c in ctrl)
        print('  A2c: subject %.2f, worst of %d controls %.2f, margin %.2f bytes'
              % (s_drop, len(ctrl), worst, s_drop - worst))
    else:
        print('  A2c: no clear control site found in %d draws -- NOT MEASURED' % tried)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
