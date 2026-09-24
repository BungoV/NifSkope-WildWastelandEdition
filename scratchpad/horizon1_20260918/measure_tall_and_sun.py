#!/usr/bin/env python
"""HORIZON1 step 1b -- the tallest placements of the region, and what the ESM
actually says about the Commonwealth's sun.

Two answers, and the second one is a REFUSAL with numbers rather than a figure
invented to fill the table.

(1) THE TALLEST.  For every drawn placement of the `.lodi`, the top of its
    world-space bound above the terrain under it:

        topAboveTerrain = pos.z + scale * (mesh.aabbMin.z + mesh.aabbExtent.z)
                          - landHeight(pos.x, pos.y)

    The bound is the mesh's own AABB, rotated placements included by taking the
    AABB's highest corner under the stored rotation, so the number is an upper
    bound on the geometry, never below it.  The land height comes from the same
    `.lodi` chunk table the writer wrote (`zMin`), and independently from the
    ESM's LAND records when --esm is given; the two are printed side by side.

(2) THE SUN.  The tree's ESM reader (src/esmdata.cpp) reads no CLMT and no
    WTHR; grep for `CLMT` there returns nothing.  So this script walks the
    plugin itself for the worldspace's climate and prints WHAT IS STORED --
    the TNAM timing struct's four times -- and states plainly that FO4 stores
    no sun ELEVATION anywhere: the elevation is the engine's own function of
    time of day and the sun path.  The reach therefore cannot be read out of
    the file; it is chosen from an elevation BAR, and this script tables
    height/tan(elevation) so the bar is picked against the tallest thing that
    actually stands in the region.

usage: measure_tall_and_sun.py <ws>.lodo <ws>.lodi [--esm <Fallout4.esm>]
                               [--worldspace 3C] [--top N] [--json out.json]
"""
import argparse
import json
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tools'))
import lodgen_native_decode as D            # noqa: E402
import lod_emission_probe as P              # noqa: E402

CHUNK_UNITS = 16384.0


# --- the ESM's own LAND heights, so "above the terrain" means the terrain ----
#
# FO4 LAND VHGT (unchanged from FO3/Skyrim): f32 offset, then 33 x 33 int8
# deltas, row-major, scaled by 8.  Column 0 of each row accumulates DOWN the
# column from `offset`; every other cell accumulates ALONG its row.  Written
# from the format alone, and its control is below: the mean of every sampled
# cell must land inside the .lodi chunk table's [zMin, zMin + zExtent] band,
# which the C++ writer computed from the same records by a different route.
HEIGHT_SCALE = 8.0


def read_land(esm_path, worldspace):
    buf = open(esm_path, 'rb').read()
    out = {}
    cur = None
    for typ, form, flags, doff, dsize, stack in P.walk(buf):
        if typ == b'CELL':
            data = P.recordData(buf, doff, dsize, flags)
            cur = None
            for st, sd in P.subrecords(data):
                if st == b'XCLC' and len(sd) >= 8:
                    cur = struct.unpack_from('<ii', sd, 0)
        elif typ == b'LAND' and cur is not None:
            data = P.recordData(buf, doff, dsize, flags)
            for st, sd in P.subrecords(data):
                if st == b'VHGT' and len(sd) >= 4 + 33 * 33:
                    off = struct.unpack_from('<f', sd, 0)[0]
                    d = sd[4:4 + 33 * 33]
                    hs = [[0.0] * 33 for _ in range(33)]
                    col = off
                    for row in range(33):
                        col += struct.unpack_from('<b', d, row * 33)[0]
                        v = col
                        hs[row][0] = v * HEIGHT_SCALE
                        for c in range(1, 33):
                            v += struct.unpack_from('<b', d, row * 33 + c)[0]
                            hs[row][c] = v * HEIGHT_SCALE
                    out[cur] = hs
            cur = None
    return out


def land_height(land, wx, wy):
    """Bilinear over the 128-unit LAND nodes; None where no LAND record."""
    cx = int(math.floor(wx / 4096.0))
    cy = int(math.floor(wy / 4096.0))
    hs = land.get((cx, cy))
    if hs is None:
        return None
    fx = (wx - cx * 4096.0) / 128.0
    fy = (wy - cy * 4096.0) / 128.0
    ix = min(31, max(0, int(fx)))
    iy = min(31, max(0, int(fy)))
    tx, ty = fx - ix, fy - iy
    h00, h10 = hs[iy][ix], hs[iy][ix + 1]
    h01, h11 = hs[iy + 1][ix], hs[iy + 1][ix + 1]
    return (h00 * (1 - tx) + h10 * tx) * (1 - ty) + (h01 * (1 - tx) + h11 * tx) * ty



def chunk_of(T, idx):
    """(chunkX, chunkY) of instance table index `idx`, north-up, per
    src/lodifile.cpp lodiChunkAt (root MISTAKES 2026-09-18 08:0x: the table is
    NORTH-up; walking it south-up quadruples the population)."""
    h = T['header']
    w = h['chunkEast'] - h['chunkWest'] + 1
    for i, c in enumerate(T['chunks']):
        if c['instanceCount'] and c['instanceFirst'] <= idx < c['instanceFirst'] + c['instanceCount']:
            return (h['chunkWest'] + i % w, h['chunkNorth'] - i // w, c)
    return (None, None, None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('lodi')
    ap.add_argument('--esm')
    ap.add_argument('--worldspace', default='3C')
    ap.add_argument('--top', type=int, default=10)
    ap.add_argument('--json')
    ap.add_argument('--control-red', action='store_true',
                    help='also run the deliberately broken decoder, which must go red')
    a = ap.parse_args()

    L = D.read_lodo(a.lodo)
    T = D.read_lodi(a.lodi)
    h = T['header']
    w = h['chunkEast'] - h['chunkWest'] + 1

    # index -> chunk, in one pass (the per-instance search above is O(n*m))
    idx_chunk = [None] * h['instanceCount']
    for i, c in enumerate(T['chunks']):
        if not c['instanceCount']:
            continue
        cx = h['chunkWest'] + i % w
        cy = h['chunkNorth'] - i // w
        for k in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
            if k < len(idx_chunk):
                idx_chunk[k] = (cx, cy, c)

    land = read_land(a.esm, a.worldspace) if a.esm else {}
    if land:
        print('LAND records read: %d cells' % len(land))

    rows = []
    for i, r in enumerate(T['instances']):
        ci = idx_chunk[i] if i < len(idx_chunk) else None
        if not ci:
            continue
        cx, cy, c = ci
        b = L['bases'][r['baseId']] if r['baseId'] < len(L['bases']) else None
        if not b:
            continue
        mid = b['rep0']
        if mid == 0xFFFF or mid >= len(L['meshes']):
            continue
        mesh = L['meshes'][mid]
        s = r['scale'] / 8192.0
        wx = cx * CHUNK_UNITS + r['px'] / 65535.0 * CHUNK_UNITS
        wy = cy * CHUNK_UNITS + r['py'] / 65535.0 * CHUNK_UNITS
        wz = c['zMin'] + r['pz'] / 65535.0 * c['zExtent']
        rot = D.unpack_rotation(r['r0'], r['r1'], r['r2'])
        # highest corner of the rotated, scaled AABB above the origin
        mn = mesh['aabbMin']
        ex = mesh['aabbExtent']
        top = -1e30
        for bx in (0, 1):
            for by in (0, 1):
                for bz in (0, 1):
                    p = (mn[0] + bx * ex[0], mn[1] + by * ex[1], mn[2] + bz * ex[2])
                    z = (rot[6] * p[0] + rot[7] * p[1] + rot[8] * p[2]) * s
                    if z > top:
                        top = z
        lh = land_height(land, wx, wy) if land else None
        rows.append({
            'i': i,
            'land_z': lh,
            'top_over_land': (wz + top - lh) if lh is not None else None,
            'model': L['string_at'](mesh['modelStringOffset']),
            'x': wx, 'y': wy, 'z': wz,
            'scale': s,
            'top_over_origin': top,
            'top_z': wz + top,
            'chunk_zmin': c['zMin'],
            'top_over_chunk_zmin': wz + top - c['zMin'],
        })

    # THE CONTROL, and the first one chosen was NOT an invariant.
    #
    # Rejected: "every sampled land height sits inside the chunk table's
    # [zMin, zMin+zExtent] band".  It came out 29,262 in / 3,861 out (88.3%) and
    # that is CORRECT behaviour, not a decoder fault: the chunk band is built
    # from INSTANCE origins, so ground below the lowest placement is legitimately
    # outside it.  A check that a right answer fails is not a check
    # (CONSTITUTION rule 4).
    #
    # Kept: THE SHARED CELL EDGE.  Cell (cx,cy)'s column 32 is the same row of
    # world positions as cell (cx+1,cy)'s column 0, and (cx,cy)'s row 32 is
    # (cx,cy+1)'s row 0.  Bethesda's VHGT stores them independently, so they
    # agree only if the accumulation order is right.  Transposing the two
    # accumulations -- the single most likely way to get this format wrong --
    # breaks every one of them, which is shown below under --control-red.
    def edge_check(L):
        ok = bad = 0
        worst = 0.0
        for (cx, cy), hs in L.items():
            e = L.get((cx + 1, cy))
            if e is not None:
                for r in range(33):
                    d = abs(hs[r][32] - e[r][0])
                    worst = max(worst, d)
                    if d <= 0.5:
                        ok += 1
                    else:
                        bad += 1
            n = L.get((cx, cy + 1))
            if n is not None:
                for c in range(33):
                    d = abs(hs[32][c] - n[0][c])
                    worst = max(worst, d)
                    if d <= 0.5:
                        ok += 1
                    else:
                        bad += 1
        return ok, bad, worst

    if land:
        ok, bad, worst = edge_check(land)
        print('CONTROL shared-cell-edge: %d agree, %d differ, worst %.3f u'
              % (ok, bad, worst))
        if a.control_red:
            # the deliberately broken decoder: accumulate along the COLUMN
            # instead of along the row.  It must go red.
            broken = {}
            for k, hs in land.items():
                broken[k] = [[hs[c][r] for c in range(33)] for r in range(33)]
            ok2, bad2, worst2 = edge_check(broken)
            print('CONTROL RED (transposed accumulation): %d agree, %d differ, '
                  'worst %.3f u' % (ok2, bad2, worst2))

    key = 'top_over_land' if any(d['top_over_land'] is not None for d in rows)         else 'top_over_chunk_zmin'
    rows.sort(key=lambda d: -(d[key] if d[key] is not None else -1e30))
    top_rows = rows[:a.top]

    print('instances %d  drawn-with-mesh %d' % (h['instanceCount'], len(rows)))
    print('--- tallest %d by bound top above the terrain under them ---' % a.top)
    for d in top_rows:
        print('%8.0f u over land (%8.0f over chunk zMin)  %-46s  (%.0f, %.0f, %.0f) scale %.2f'
              % (d[key] if d[key] is not None else -1, d['top_over_chunk_zmin'],
                 d['model'][:46], d['x'], d['y'], d['z'], d['scale']))

    tallest = top_rows[0][key] if top_rows and top_rows[0][key] is not None else         (top_rows[0]['top_over_chunk_zmin'] if top_rows else 0.0)

    # --- the reach table -----------------------------------------------------
    print('--- reach = height / tan(elevation), for the tallest %.0f u ---' % tallest)
    reach = {}
    for deg in (1, 2, 3, 5, 7.5, 10, 15, 20, 30, 45, 60):
        t = math.tan(math.radians(deg))
        reach[deg] = tallest / t if t > 0 else float('inf')
        print('  %5.1f deg -> %12.0f u  (%.2f km at 64 u/m ... %.0f game units)'
              % (deg, reach[deg], reach[deg] / 64.0 / 1000.0, reach[deg]))

    # --- what the ESM stores about the sun ----------------------------------
    clmt = []
    if a.esm:
        buf = open(a.esm, 'rb').read()
        wsform = int(a.worldspace, 16)
        ws_climate = None
        for typ, form, flags, doff, dsize, stack in P.walk(buf):
            if typ == b'WRLD' and form == wsform:
                data = P.recordData(buf, doff, dsize, flags)
                for st, sd in P.subrecords(data):
                    if st == b'CNAM' and len(sd) >= 4:
                        ws_climate = struct.unpack_from('<I', sd, 0)[0]
            if typ == b'CLMT':
                data = P.recordData(buf, doff, dsize, flags)
                row = {'form': form}
                for st, sd in P.subrecords(data):
                    if st == b'EDID':
                        row['edid'] = P.zstr(sd)
                    elif st == b'TNAM' and len(sd) >= 6:
                        # Sunrise Begin / Sunrise End / Sunset Begin / Sunset End,
                        # each in units of 10 minutes; then Volatility, Moons.
                        row['tnam'] = list(sd[:6])
                        row['sunriseBegin_h'] = sd[0] * 10.0 / 60.0
                        row['sunriseEnd_h'] = sd[1] * 10.0 / 60.0
                        row['sunsetBegin_h'] = sd[2] * 10.0 / 60.0
                        row['sunsetEnd_h'] = sd[3] * 10.0 / 60.0
                clmt.append(row)
        print('--- CLMT records in %s: %d ---' % (os.path.basename(a.esm), len(clmt)))
        print('worldspace %s CNAM (climate) = %s'
              % (a.worldspace, ('%08X' % ws_climate) if ws_climate else 'ABSENT'))
        for row in clmt:
            if ws_climate and row['form'] != ws_climate:
                continue
            print('  %08X %-24s TNAM %s  sunrise %.2f..%.2f h  sunset %.2f..%.2f h'
                  % (row['form'], row.get('edid', ''), row.get('tnam'),
                     row.get('sunriseBegin_h', -1), row.get('sunriseEnd_h', -1),
                     row.get('sunsetBegin_h', -1), row.get('sunsetEnd_h', -1)))
        print('NO SUN ELEVATION IS STORED. CLMT gives TIMES, not angles; the '
              'engine derives the elevation from the time of day. The reach is '
              'therefore chosen from an elevation BAR, stated as a default.')

    res = {'tallest': tallest, 'top': top_rows, 'reach_by_elevation_deg': reach,
           'clmt': clmt, 'instances': h['instanceCount'], 'drawn': len(rows)}
    if a.json:
        with open(a.json, 'w') as f:
            json.dump(res, f, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
