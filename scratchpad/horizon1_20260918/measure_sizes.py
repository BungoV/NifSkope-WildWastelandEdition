#!/usr/bin/env python
"""HORIZON1 step 1c -- the sizes, measured on the region and projected to the
whole populated worldspace.

What it measures, all from files already on disk:

  * the `.lodi`'s bytes per placement today, and the v6/v7 streams' bytes per
    placement, so a v8 horizon stream is quoted as a MULTIPLE of something that
    already ships rather than as a guess;
  * the drawn vertex count per placement (= the sky stream's length per
    placement), which is what an A-byte-per-vertex horizon stream multiplies;
  * the `.lodt` finest tile's bytes per role, from the sheet files themselves;
  * the populated extent of the worldspace, counted from the plugin's own LAND
    and REFR records -- NOT from the 192 x 192 rim, which every Commonwealth
    cell has a LAND record inside and which therefore says nothing about where
    anything stands.

usage: measure_sizes.py <ws>.lodo <ws>.lodi [--esm E --worldspace 3C]
                        [--lodt DIR] [--json out.json]
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

CELL_UNITS = 4096.0


def worldspace_population(esm, wsform):
    """(cells with a LAND record, cells with at least one drawn-eligible REFR,
    placements) for one worldspace, from the plugin alone.

    A cell counts as POPULATED when it holds at least one REFR that is neither
    deleted nor initially disabled -- the same two tests src/lodgen.cpp applies
    before a placement reaches the `.lodi`.  It is deliberately NOT "has a LAND
    record": every one of the Commonwealth's 192 x 192 cells has one of those,
    so that count is the rim and not the population."""
    buf = open(esm, 'rb').read()
    DELETED = 0x00000020
    INITIALLY_DISABLED = 0x00000800
    land_cells = set()
    refr_cells = {}
    cur = None
    in_ws = False
    for typ, form, flags, doff, dsize, stack in P.walk(buf):
        if typ == b'WRLD':
            in_ws = (form == wsform)
            cur = None
            continue
        if not in_ws:
            continue
        if typ == b'CELL':
            data = P.recordData(buf, doff, dsize, flags)
            cur = None
            for st, sd in P.subrecords(data):
                if st == b'XCLC' and len(sd) >= 8:
                    cur = struct.unpack_from('<ii', sd, 0)
        elif typ == b'LAND':
            if cur is not None:
                land_cells.add(cur)
        elif typ == b'REFR':
            if cur is None:
                continue
            if flags & (DELETED | INITIALLY_DISABLED):
                continue
            refr_cells[cur] = refr_cells.get(cur, 0) + 1
    return land_cells, refr_cells


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('lodo')
    ap.add_argument('lodi')
    ap.add_argument('--esm')
    ap.add_argument('--worldspace', default='3C')
    ap.add_argument('--lodt')
    ap.add_argument('--json')
    a = ap.parse_args()

    L = D.read_lodo(a.lodo)
    T = D.read_lodi(a.lodi)
    h = T['header']
    n = h['instanceCount']
    res = {'lodi_version': h.get('version'), 'instances': n}

    lodi_bytes = os.path.getsize(a.lodi)
    lodo_bytes = os.path.getsize(a.lodo)
    res['lodi_bytes'] = lodi_bytes
    res['lodo_bytes'] = lodo_bytes
    res['lodi_bytes_per_placement'] = lodi_bytes / max(1, n)

    vao = h.get('vertexAoBytes', 0) or 0
    vsky = h.get('vertexSkyBytes', 0) or 0
    res['vertexAoBytes'] = vao
    res['vertexSkyBytes'] = vsky

    # the drawn vertex count per placement: the v6/v7 stream's own slice lengths
    # when the file carries one, else the mesh the base's rep0 names.
    lens = []
    first = T.get('vertexAoFirst') or []
    if len(first) == n + 1:
        lens = [first[i + 1] - first[i] for i in range(n)]
    else:
        for r in T['instances']:
            b = L['bases'][r['baseId']] if r['baseId'] < len(L['bases']) else None
            if not b or b['rep0'] == 0xFFFF or b['rep0'] >= len(L['meshes']):
                lens.append(0)
                continue
            mesh = L['meshes'][b['rep0']]
            lo, hi2 = 0xFFFFFFFF, 0
            for c in range(mesh['clusterFirst'], mesh['clusterFirst'] + mesh['clusterCount']):
                cl = L['clusters'][c]
                lo = min(lo, cl['vertexBase'])
                hi2 = max(hi2, cl['vertexBase'] + cl['vertexCount'])
            lens.append(max(0, hi2 - lo))
    drawn = [x for x in lens if x]
    res['streamed_placements'] = len(drawn)
    res['drawn_vertices_total'] = sum(drawn)
    res['drawn_vertices_mean'] = sum(drawn) / max(1, len(drawn))
    res['drawn_vertices_max'] = max(drawn) if drawn else 0

    print('%s: version %s, %d placements, %d streamed, %d drawn vertices '
          '(mean %.1f a placement, max %d)'
          % (os.path.basename(a.lodi), h.get('version'), n, len(drawn),
             sum(drawn), res['drawn_vertices_mean'], res['drawn_vertices_max']))
    print('  .lodi %d B (%.1f B a placement), .lodo %d B; vertexAo %d B, vertexSky %d B'
          % (lodi_bytes, res['lodi_bytes_per_placement'], lodo_bytes, vao, vsky))

    # --- the v8 horizon stream, projected -----------------------------------
    V = sum(drawn)
    res['horizon_stream'] = {}
    print('--- v8 object horizon stream over this region, by azimuth count ---')
    for A in (8, 16, 32):
        body = V * A
        offs = 4 * (len(drawn) and n + 1)
        tot = body + offs
        res['horizon_stream'][A] = {'bytes': tot, 'per_placement': tot / max(1, n)}
        print('  A=%2d: %10d B  (%.2f MB, %.1f B a placement, %.0f x the sky stream)'
              % (A, tot, tot / 1048576.0, tot / max(1, n),
                 tot / max(1, vsky) if vsky else 0))

    # --- the terrain sheet, projected ---------------------------------------
    # BC3 is 1 byte a texel (16 B a 4x4 block).  Two RGBA sheets carry 8 bins
    # each, so 16 bins cost 2 bytes a texel.
    print('--- role-7 terrain horizon sheet, 2 x BC3 RGBA = 2 B a texel ---')
    res['terrain_sheet'] = {}
    for texel in (8, 16, 32, 64, 128):
        per_cell = (CELL_UNITS / texel) ** 2 * 2.0
        res['terrain_sheet'][texel] = per_cell
        print('  %4d u a texel: %8.0f texels a cell, %10.0f B a cell (%.1f KB)'
              % (texel, (CELL_UNITS / texel) ** 2, per_cell, per_cell / 1024.0))

    # --- the populated worldspace -------------------------------------------
    if a.esm:
        land_cells, refr_cells = worldspace_population(a.esm, int(a.worldspace, 16))
        pop = len(refr_cells)
        placements = sum(refr_cells.values())
        res['ws_land_cells'] = len(land_cells)
        res['ws_populated_cells'] = pop
        res['ws_refrs'] = placements
        print('--- worldspace %s: %d cells with LAND, %d cells with a live REFR, '
              '%d REFRs ---' % (a.worldspace, len(land_cells), pop, placements))
        scale = placements / max(1, n)
        print('  the region holds %d of them, so the worldspace is %.1f x this region'
              % (n, scale))
        res['ws_scale'] = scale
        for A in (8, 16, 32):
            b = res['horizon_stream'][A]['bytes'] * scale
            print('  object stream A=%2d over the worldspace: %.2f GB'
                  % (A, b / 1073741824.0))
            res['horizon_stream'][A]['ws_bytes'] = b
        for texel in (8, 16, 32, 64, 128):
            b = res['terrain_sheet'][texel] * len(land_cells)
            print('  terrain sheet %4d u a texel over the worldspace: %.2f GB'
                  % (texel, b / 1073741824.0))
            res.setdefault('terrain_ws', {})[texel] = b

    # --- the finest tile's bytes per role ------------------------------------
    if a.lodt and os.path.isdir(a.lodt):
        print('--- .lodt sheets present ---')
        tot = {}
        for f in sorted(os.listdir(a.lodt)):
            p = os.path.join(a.lodt, f)
            if os.path.isfile(p):
                tot[f] = os.path.getsize(p)
        for f, b in sorted(tot.items(), key=lambda kv: -kv[1])[:20]:
            print('  %-52s %10d B' % (f[:52], b))
        res['lodt_files'] = tot

    if a.json:
        with open(a.json, 'w') as f:
            json.dump(res, f, indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
