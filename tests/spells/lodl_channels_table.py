#!/usr/bin/env python3
"""Lane CHANVIEW1 step 1 -- the channel table, read from the SHIPPED files.

One reader per format: `.lodi` / `.lodo` come from tests/spells/lodgen_native_decode.py
(read_lodi / read_lodo), the `.lodt` container and its BC1 decode come from
tests/spells/lodgen_vt_check.py (Lodv / decode_bc1).  Nothing is re-implemented here
except the BC3 ALPHA block, which no existing reader decodes (the mask sheet's A is the
ground cover and the AO reader only ever wanted B).

The POPULATION is the one the viewer draws, not the whole file:

    region      cells [x0,y0]..[x1,y1]      (WW_LODL_REGION)
    slot        base.rep[WW_LODI_SLOT]      (0 in every picture this lane takes)
    level       clusterLods[c].level == L   (plus the root-below rule, inert at L=0)

usage: channel_table.py <lodi> <lodt> <x0> <y0> <x1> <y1> [slot] [level]
"""
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tests', 'spells'))

import lodgen_native_decode as ND          # noqa: E402
import lodgen_vt_check as VT               # noqa: E402

NO_MESH = 0xFFFF
AO_UNMEASURED = 0xFF


def stats(values):
    """min / max / mean / count-of-distinct, and the constant verdict."""
    if not values:
        return dict(n=0, min=None, max=None, mean=None, distinct=0, constant=None)
    lo, hi = min(values), max(values)
    d = len(set(values))
    return dict(n=len(values), min=lo, max=hi,
                mean=sum(values) / float(len(values)), distinct=d,
                constant=(lo if d == 1 else None))


def drawn(L, T, x0, y0, x1, y1, slot, level):
    """Replicate src/lodinative.cpp's selection exactly."""
    h = T['header']
    cellW = h['chunkCells']
    w = h['chunkEast'] - h['chunkWest'] + 1
    out = []
    for ci, ch in enumerate(T['chunks']):
        if not ch['instanceCount']:
            continue
        chunkX = h['chunkWest'] + (ci % w)
        chunkY = h['chunkNorth'] - (ci // w)
        cx0, cy0 = chunkX * cellW, chunkY * cellW
        if cx0 + cellW - 1 < x0 or cx0 > x1 or cy0 + cellW - 1 < y0 or cy0 > y1:
            continue
        for k in range(ch['instanceCount']):
            ii = ch['instanceFirst'] + k
            inst = T['instances'][ii]
            x = chunkX * 16384.0 + inst['px'] / 65535.0 * 16384.0
            y = chunkY * 16384.0 + inst['py'] / 65535.0 * 16384.0
            if not (x0 <= int(x // 4096) <= x1 and y0 <= int(y // 4096) <= y1):
                continue
            if inst['baseId'] >= len(L['bases']):
                continue
            base = L['bases'][inst['baseId']]
            reps = [base['rep0'], base['rep1'], base['rep2'], base['rep3']]
            mesh = NO_MESH
            if slot >= 0:
                if reps[slot] != NO_MESH and reps[slot] < len(L['meshes']):
                    mesh = reps[slot]
            else:
                for r in reps:
                    if r != NO_MESH and r < len(L['meshes']):
                        mesh = r
                        break
            if mesh == NO_MESH:
                continue
            out.append((ii, inst, mesh))
    return out


def mesh_vertex_indices(L, mesh, level):
    """Library vertex indices of the clusters the viewer keeps, in its order."""
    m = L['meshes'][mesh]
    idx = []
    for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        if c >= len(L['clusters']):
            break
        cll = L['clusterLods'][c]
        root_below = cll['level'] < level and cll['parentCount'] == 0
        if cll['level'] != level and not root_below:
            continue
        cl = L['clusters'][c]
        for v in range(cl['vertexCount']):
            vi = cl['vertexBase'] + v
            if vi >= len(L['vertices']):
                break
            idx.append(vi)
    return idx


def mesh_range(L, mesh):
    m = L['meshes'][mesh]
    lo, hi = 0xFFFFFFFF, 0
    for c in range(m['clusterFirst'], m['clusterFirst'] + m['clusterCount']):
        if c >= len(L['clusters']):
            break
        cl = L['clusters'][c]
        lo = min(lo, cl['vertexBase'])
        hi = max(hi, cl['vertexBase'] + cl['vertexCount'])
    return lo, (hi - lo if hi > lo else 0)


# ---------------------------------------------------------------- mask sheet
def bc1_channel(block, ch):
    """The 16 texels' R, G or B out of one BC1 colour block (8 bytes)."""
    c0 = block[0] | (block[1] << 8)
    c1 = block[2] | (block[3] << 8)

    def comp(c):
        if ch == 0:
            v = (c >> 11) & 31
            return (v << 3) | (v >> 2)
        if ch == 1:
            v = (c >> 5) & 63
            return (v << 2) | (v >> 4)
        v = c & 31
        return (v << 3) | (v >> 2)
    a, b = comp(c0), comp(c1)
    if c0 > c1:
        pal = [a, b, (2 * a + b) // 3, (a + 2 * b) // 3]
    else:
        pal = [a, b, (a + b) // 2, 0]
    bits = struct.unpack('<I', block[4:8])[0]
    return [pal[(bits >> (2 * i)) & 3] for i in range(16)]


def bc3_alpha(block):
    """The 16 texels' A out of one BC3 alpha block (the first 8 bytes)."""
    a0, a1 = block[0], block[1]
    if a0 > a1:
        pal = [a0, a1] + [((6 - i) * a0 + (1 + i) * a1) // 7 for i in range(6)]
    else:
        pal = [a0, a1] + [((4 - i) * a0 + (1 + i) * a1) // 5 for i in range(4)] + [0, 255]
    bits = int.from_bytes(block[2:8], 'little')
    return [pal[(bits >> (3 * i)) & 7] for i in range(16)]


def sheet_plane(v, index, si, ch):
    """One tile's one channel of one sheet, mip 0, as `stored x stored` bytes."""
    e = v.table[index]
    p = v.payload(index)
    if p is None:
        return None, 'tile absent'
    cover = bool(e['flags'] & 2)
    sd = v.sheets[si]
    fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
    if fmt in (71, 72):
        bb = 8
    elif fmt in (77, 78):
        bb = 16
    else:
        return None, 'format %d is not BC1 or BC3' % fmt
    if ch == 3 and bb == 8:
        return None, 'the sheet is BC1 (dxgi %d) on this tile: it carries no alpha' % fmt
    o = v.sheetOffset(cover, si, 0)
    dim = v.stored
    blocks = dim // 4
    plane = bytearray(dim * dim)
    for by in range(blocks):
        for bx in range(blocks):
            base = o + (by * blocks + bx) * bb
            blk = p[base:base + bb]
            texels = bc3_alpha(blk[:8]) if ch == 3 else bc1_channel(blk[bb - 8:], ch)
            for i in range(16):
                plane[(by * 4 + (i >> 2)) * dim + bx * 4 + (i & 3)] = texels[i]
    return plane, 'dxgi %d' % fmt


def sheet_channel(lodt, role, ch, tiles_wanted=None):
    """Every CONTENT texel of every present tile of one sheet's one channel."""
    v = VT.Lodv(lodt)
    si = None
    for i in range(v.sheetCount):
        if v.sheets[i]['role'] == role:
            si = i
    if si is None:
        return None, 'the container carries no sheet with role %d' % role
    dim, border = v.stored, v.border
    vals = []
    why = set()
    for ty in range(v.tilesY):
        for tx in range(v.tilesX):
            if tiles_wanted is not None and (tx, ty) not in tiles_wanted:
                continue
            index = ty * v.tilesX + tx
            if not (v.table[index]['flags'] & 1):
                continue
            plane, w = sheet_plane(v, index, si, ch)
            if plane is None:
                return None, 'tile %d,%d: %s' % (tx, ty, w)
            why.add(w)
            for y in range(border, dim - border):
                vals.extend(plane[y * dim + border:y * dim + dim - border])
    return vals, ', '.join(sorted(why))


def main():
    lodi = sys.argv[1]
    lodt = sys.argv[2]
    x0, y0, x1, y1 = (int(a) for a in sys.argv[3:7])
    slot = int(sys.argv[7]) if len(sys.argv) > 7 else 0
    level = int(sys.argv[8]) if len(sys.argv) > 8 else 0

    lodo = os.path.splitext(lodi)[0] + '.lodo'
    L = ND.read_lodo(lodo)
    T = ND.read_lodi(lodi)
    sel = drawn(L, T, x0, y0, x1, y1, slot, level)

    per_place = {k: [] for k in ('identity', 'identityraw', 'identitylow',
                                 'sky', 'ground', 'seed', 'placementao')}
    sway, selfao, vao = [], [], []
    seed_trees = 0
    vao_bytes = 0
    for ii, inst, mesh in sel:
        cold = T['cold'][ii]
        per_place['identity'].append(cold['identity'])
        per_place['identityraw'].append(cold['identity'])
        # what the viewer's `identityraw` DRAWS: the identity's low byte. The u16
        # itself over 0..65535 would put this chunk's 0..2448 at black.
        per_place['identitylow'].append(cold['identity'] & 0xFF)
        per_place['sky'].append(inst['sky'])
        per_place['ground'].append(inst['ground'])
        per_place['seed'].append(inst['seed'])
        if inst['seed']:
            seed_trees += 1
        pa = T['placementAo'][ii] if ii < len(T['placementAo']) else AO_UNMEASURED
        per_place['placementao'].append(pa)
        vidx = mesh_vertex_indices(L, mesh, level)
        for vi in vidx:
            sway.append(L['vertices'][vi]['sway'])
            selfao.append(L['vertices'][vi]['selfAO'])
        if T['vertexAoFirst'] and ii + 1 < len(T['vertexAoFirst']):
            f, l = T['vertexAoFirst'][ii], T['vertexAoFirst'][ii + 1]
            first, span = mesh_range(L, mesh)
            if l > f and span == l - f:
                vao_bytes += l - f
                for vi in vidx:
                    vao.append(T['vertexAo'][f + vi - first])

    out = {'population': {'placements': len(sel), 'vertices': len(sway),
                          'vertexAoBytes': vao_bytes, 'seedNonZero': seed_trees,
                          'region': [x0, y0, x1, y1], 'slot': slot, 'level': level}}
    for k, v in per_place.items():
        out[k] = stats(v)
    out['sway'] = stats(sway)
    out['selfao'] = stats(selfao)
    out['ao'] = stats(vao)

    for name, role, ch in (('mask-r', 5, 0), ('mask-g', 5, 1), ('mask-b', 5, 2), ('mask-a', 5, 3),
                           ('normal-r', 2, 0), ('normal-g', 2, 1), ('normal-b', 2, 2),
                           ('emissive-r', 6, 0)):
        vals, why = sheet_channel(lodt, role, ch)
        if vals is None:
            out[name] = {'absent': why}
        else:
            s = stats(vals)
            s['note'] = why
            out[name] = s
    print(json.dumps(out, indent=1, sort_keys=True))


if __name__ == '__main__':
    main()
