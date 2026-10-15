#!/usr/bin/env python3
"""Independent reader and twin for `.lodp` v1 (LOD PRTP patches, lane LODP1, 2026-10-15).

Shares no code with src/lodpfile.cpp or src/lodpplace.cpp: struct + zlib + numpy only.
The `.lodo` is read by lodgen_native_decode.py (itself independent of the C++).

  lodp_decode.py <file.lodp>...
      read each file with every rule of the contract (docs/LODGEN_NATIVE_LODO_LODI.md
      section 13); print ACCEPTED or REFUSED <the rule>.

  lodp_decode.py --fixture <dir> [--expect-red faces|area]
      the known-answer fixture (`--lodp-fixture` / lodp_gate.exe write): reads Synthetic.lodo,
      placements.tsv and every lodp_*.lodp, then
        S  the `.lodo` v8 chart stream equals this file's own chart rule, triangle for triangle;
        T  a full twin placement (double maths in the C++ operation order) gives the SAME BYTES;
        F  every object patch lies on a level-0 triangle of its own chart (<= 0.45 u);
        A  every chart's patch areas sum to its triangles' area x scale^2 (rel <= 2e-3);
        B  every object patch sits inside the slot that names it (binding);
        Z  every terrain patch stands on the ground's centre sample (x, y, z exact to 0.25 u);
        E  the expect file the fixture wrote BEFORE the run.
      --expect-red names the gate that must FAIL (red controls fxr1 / fxr2): the run passes
      only when that gate failed and says so.

  lodp_decode.py --controls <good.lodp> <outdir>
      tier-2 mutation controls: flip one field, re-sign both CRCs, so the RULE refuses; writes
      the mutants plus controls.tsv (file, the substring the refusal must carry) and prints this
      reader's verdict on each.
  lodp_decode.py --judge <controls.tsv> <reader output>
      checks the other reader's REFUSED lines name the same rule.

  lodp_decode.py --stream <file.lodo>
      recompute the chart stream of a real `.lodo` v8 and compare (gate S on a real library).

  lodp_decode.py --real <ws.lodo> <ws.lodi> <lodp dir> [--sample N]
      gates on a REAL bake's sector files (`lodgen --lodp`), placements read from the `.lodi`
      by lodgen_native_decode.py: R every file accepted; I every instance row names a `.lodi`
      placement drawing that mesh (the base's first present rep), sorted, none Initially
      Disabled; P every such placement in the files' cells whose mesh has charts has its row
      (both directions); then F, B, A (as in --fixture) on N instance rows spread evenly.
"""
import math
import os
import struct
import sys
import zlib

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_native_decode as nd  # noqa: E402  (the independent .lodo decoder)

MAGIC = 0x50444F4C
VERSION = 1
HDR = 256
SECTOR_CELLS = 16
NCELLS = 256
CELL_UNITS = 4096.0
NONE = 0xFFFFFFFF
POS_STEP = 0.5
AREA_UNIT = 4096.0
CREASE_COS = 0.70710678118654752440
CREASE_TIE = 1e-9  # an exact 45-degree bevel always joins (src/lodpfile.h LODP_CREASE_TIE)
CHART_MAX = 0xFC
FOLIAGE = 0xFE
CNONE = 0xFF
K_TERRAIN, K_OBJECT, K_WATER, K_TREE = 0, 1, 2, 3
SUBMERGED = 2
CELL_TERRAIN = 1
DEGENERATE = 1e-6
MAT_TREE = 4

CELL = struct.Struct('<IIIIQfHH')                 # 32
INST = struct.Struct('<IIHHI')                    # 16
CHART = struct.Struct('<IHHHBBf3f3f3fIII')        # 64
PATCH = struct.Struct('<3hH3BBHBB16s16sIHHII')    # 64
LINK = struct.Struct('<IHH')                      # 8
assert (CELL.size, INST.size, CHART.size, PATCH.size, LINK.size) == (32, 16, 64, 64, 8)


class Refusal(Exception):
    pass


def crc32(b):
    return zlib.crc32(b) & 0xFFFFFFFF


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


def half_bits(x):
    """float32 -> IEEE half, round to nearest even (numpy's own conversion, not the C++ one)."""
    return int(np.array([np.float32(x)]).astype(np.float16).view(np.uint16)[0])


def half_val(h):
    return float(np.array([h], dtype=np.uint16).view(np.float16)[0])


def fnv(data, h=0xCBF29CE484222325):
    return nd.fnv1a64(data, h)


def options_hash(patch_units, grid):
    h = fnv(b'lodp-options')
    h = fnv(struct.pack('<I', VERSION), h)
    h = fnv(struct.pack('<f', patch_units), h)
    h = fnv(struct.pack('<i', grid), h)
    h = fnv(struct.pack('<d', CREASE_COS), h)
    return h


def sector_of(c):
    return c // SECTOR_CELLS      # Python floors: the same as the contract's floor(c / 16)


# ----------------------------------------------------------------- the reader
def layout(ni, nc, ns, np_, nl):
    o = {'cell': HDR}
    o['inst'] = o['cell'] + NCELLS * 32
    o['chart'] = o['inst'] + ni * 16
    o['slot'] = o['chart'] + nc * 64
    o['patch'] = o['slot'] + ns * 4
    o['link'] = o['patch'] + np_ * 64
    o['end'] = o['link'] + nl * 8
    return o


def read_lodp(b):
    if len(b) < HDR:
        raise Refusal('file is %d bytes, shorter than the 256-byte header' % len(b))
    magic, ver, flags, hcrc = struct.unpack_from('<IIII', b, 0)
    if magic != MAGIC:
        raise Refusal('magic %r is not LODP' % b[:4])
    if ver != VERSION:
        raise Refusal('version %d is not one this reader knows (1)' % ver)
    if crc32(b[0x10:HDR]) != hcrc:
        raise Refusal('headerCrc32 does not match the header')
    if b[0x3F]:
        raise Refusal('reserved header byte at 0x3f is not 0')
    if struct.unpack_from('<I', b, 0x5C)[0]:
        raise Refusal('reserved header word at 0x5c is not 0')
    for k in range(0x98, HDR):
        if b[k]:
            raise Refusal('reserved header byte at 0x%x is not 0' % k)
    ws, inp, lodo, opts = struct.unpack_from('<QQQQ', b, 0x10)
    sx, sy, pu, az, grid = struct.unpack_from('<iifHB', b, 0x30)
    if az != 16:
        raise Refusal('azimuth count %d is not 16' % az)
    ncell, ni, nc, ns, np_, nl, icrc = struct.unpack_from('<7I', b, 0x40)
    if ncell != NCELLS:
        raise Refusal('cell count %d is not 256' % ncell)
    L = layout(ni, nc, ns, np_, nl)
    offs = struct.unpack_from('<7Q', b, 0x60)
    for got, (name, key) in zip(offs, (('cell table', 'cell'), ('instance table', 'inst'), ('chart table', 'chart'),
                                       ('slot table', 'slot'), ('patch table', 'patch'), ('link table', 'link'),
                                       ('fileBytes', 'end'))):
        if got != L[key]:
            raise Refusal('%s at %d, the layout puts it at %d' % (name, got, L[key]))
    if len(b) != L['end']:
        raise Refusal('file is %d bytes, the header says %d' % (len(b), L['end']))
    if crc32(b[HDR:]) != icrc:
        raise Refusal('indexCrc32 does not match the tables')
    S = {'flags': flags, 'worldspaceHash': ws, 'lodInputHash': inp, 'lodoIdentity': lodo, 'optionsHash': opts,
         'sectorX': sx, 'sectorY': sy, 'patchUnits': pu, 'terrainGrid': grid,
         'headerCrc32': hcrc, 'indexCrc32': icrc, 'fileBytes': L['end']}
    S['cells'] = [dict(zip(('firstPatch', 'patchCount', 'firstChart', 'chartCount', 'inputHash', 'zBase', 'flags',
                            'reserved'), CELL.unpack_from(b, L['cell'] + 32 * i))) for i in range(NCELLS)]
    S['instances'] = [dict(zip(('lodiIndex', 'firstChart', 'chartCount', 'reserved', 'lodoMesh'),
                               INST.unpack_from(b, L['inst'] + 16 * i))) for i in range(ni)]
    S['charts'] = []
    for i in range(nc):
        v = CHART.unpack_from(b, L['chart'] + 64 * i)
        S['charts'].append({'firstSlot': v[0], 'nu': v[1], 'nv': v[2], 'nw': v[3], 'meshChart': v[4], 'kind': v[5],
                            'step': v[6], 'origin': v[7:10], 'axisU': v[10:13], 'axisN': v[13:16],
                            'instance': v[16], 'fallbackPatch': v[17], 'reserved': v[18]})
    S['slots'] = list(struct.unpack_from('<%dI' % ns, b, L['slot']))
    S['patches'] = []
    for i in range(np_):
        v = PATCH.unpack_from(b, L['patch'] + 64 * i)
        S['patches'].append({'pos': v[0:3], 'normal': v[3], 'albedo': v[4:7], 'flags': v[7], 'area': v[8],
                             'sampleCount': v[9], 'kind': v[10], 'horizon': v[11], 'openness': v[12],
                             'firstLink': v[13], 'linkCount': v[14], 'push': v[15], 'lodSource': v[16],
                             'reserved': v[17]})
    S['links'] = [LINK.unpack_from(b, L['link'] + 8 * i) for i in range(nl)]
    check_sector(S)
    if opts != options_hash(pu, grid):
        raise Refusal('optionsHash does not match the patch size and terrain grid the header states')
    return S


def check_sector(S):
    if S['flags'] & ~1:
        raise Refusal('header flags 0x%x carry unknown bits' % S['flags'])
    if not (S['patchUnits'] > 0) or not math.isfinite(S['patchUnits']):
        raise Refusal('patch size %r is not positive' % S['patchUnits'])
    G = S['terrainGrid']
    if not 1 <= G <= 64:
        raise Refusal('terrain grid %d is outside 1..64' % G)
    P, C, I, SL = S['patches'], S['charts'], S['instances'], S['slots']
    np_, nc = len(P), len(C)
    nextP = nextC = 0
    cellOfChart = [None] * nc
    for ci, c in enumerate(S['cells']):
        if c['reserved']:
            raise Refusal('cell %d: reserved word is %d, not 0' % (ci, c['reserved']))
        if c['flags'] & 2:
            raise Refusal('cell %d: quest-layer flag is PROPOSED (P1) and refused in v1' % ci)
        if c['flags'] & ~1:
            raise Refusal('cell %d: flags 0x%x carry unknown bits' % (ci, c['flags']))
        if not math.isfinite(c['zBase']):
            raise Refusal('cell %d: zBase is not finite' % ci)
        if c['firstPatch'] != nextP:
            raise Refusal('cell %d: first patch %d, but the cells before it end at %d (patches are in cell order)'
                          % (ci, c['firstPatch'], nextP))
        if c['firstChart'] != nextC:
            raise Refusal('cell %d: first chart %d, but the cells before it end at %d (charts are in cell order)'
                          % (ci, c['firstChart'], nextC))
        if c['firstPatch'] + c['patchCount'] > np_:
            raise Refusal('cell %d: patch range runs past %d patches' % (ci, np_))
        if c['firstChart'] + c['chartCount'] > nc:
            raise Refusal('cell %d: chart range runs past %d charts' % (ci, nc))
        terr = G * G if c['flags'] & CELL_TERRAIN else 0
        if c['patchCount'] < terr:
            raise Refusal('cell %d: terrain flag set but only %d patches' % (ci, c['patchCount']))
        for k in range(c['patchCount']):
            p = P[c['firstPatch'] + k]
            isT = p['kind'] == K_TERRAIN
            if (k < terr) != isT:
                raise Refusal('cell %d patch %d: kind %d where the terrain grid says %s'
                              % (ci, c['firstPatch'] + k, p['kind'], 'terrain' if k < terr else 'not terrain'))
            if isT and p['lodSource'] != k:
                raise Refusal('cell %d patch %d: terrain lodSource %d, not its grid index %d'
                              % (ci, c['firstPatch'] + k, p['lodSource'], k))
            if not isT and not c['firstChart'] <= p['lodSource'] < c['firstChart'] + c['chartCount']:
                raise Refusal("cell %d patch %d: lodSource chart %d is not one of the cell's charts"
                              % (ci, c['firstPatch'] + k, p['lodSource']))
        for k in range(c['chartCount']):
            cellOfChart[c['firstChart'] + k] = ci
        nextP += c['patchCount']
        nextC += c['chartCount']
    if nextP != np_:
        raise Refusal('the cells hold %d patches, the table %d' % (nextP, np_))
    if nextC != nc:
        raise Refusal('the cells hold %d charts, the table %d' % (nextC, nc))
    instOf = [None] * nc
    for i, r in enumerate(I):
        if r['reserved']:
            raise Refusal('instance %d: reserved word is %d, not 0' % (i, r['reserved']))
        if i and not I[i - 1]['lodiIndex'] < r['lodiIndex']:
            raise Refusal('instance table is not sorted by lodiIndex ascending at row %d' % i)
        if r['chartCount'] == 0:
            raise Refusal('instance %d owns no chart' % i)
        if r['firstChart'] + r['chartCount'] > nc:
            raise Refusal('instance %d: chart range runs past %d charts' % (i, nc))
        c0 = cellOfChart[r['firstChart']]
        for k in range(r['chartCount']):
            ch = r['firstChart'] + k
            if instOf[ch] is not None:
                raise Refusal('chart %d is claimed by instances %d and %d' % (ch, instOf[ch], i))
            instOf[ch] = i
            if cellOfChart[ch] != c0:
                raise Refusal('instance %d: its charts span cells %d and %d' % (i, c0, cellOfChart[ch]))
            c = C[ch]
            if c['kind'] == 1:
                if k + 1 != r['chartCount']:
                    raise Refusal("instance %d: tree chart %d is not the instance's last chart" % (i, ch))
                if c['meshChart'] != FOLIAGE:
                    raise Refusal('chart %d: a tree chart must carry id 0xfe' % ch)
            elif c['meshChart'] != k:
                raise Refusal('chart %d: mesh chart id %d is not its place %d in instance %d' % (ch, c['meshChart'], k, i))
    for ch in range(nc):
        if instOf[ch] is None:
            raise Refusal('chart %d belongs to no instance' % ch)
    nextS = 0
    refs = [0] * np_
    for ch, c in enumerate(C):
        if c['reserved']:
            raise Refusal('chart %d: reserved word is %d, not 0' % (ch, c['reserved']))
        if c['instance'] != instOf[ch]:
            raise Refusal('chart %d: names instance %d, but instance %d owns it' % (ch, c['instance'], instOf[ch]))
        if c['kind'] not in (0, 1):
            raise Refusal('chart %d: kind %d is neither surface nor tree' % (ch, c['kind']))
        if c['kind'] == 0 and c['meshChart'] > CHART_MAX:
            raise Refusal('chart %d: surface chart id %d is above %d' % (ch, c['meshChart'], CHART_MAX))
        if not (c['nu'] and c['nv'] and c['nw']):
            raise Refusal('chart %d: slot grid is empty' % ch)
        if c['kind'] == 1 and (c['nu'], c['nv'], c['nw']) != (1, 1, 1):
            raise Refusal('chart %d: a tree chart is one slot' % ch)
        if not (c['step'] > 0) or not math.isfinite(c['step']):
            raise Refusal('chart %d: step %r is not positive' % (ch, c['step']))
        vals = c['origin'] + c['axisU'] + c['axisN']
        if not all(math.isfinite(v) for v in vals):
            raise Refusal('chart %d: frame is not finite' % ch)
        lu = sum(v * v for v in c['axisU'])
        ln = sum(v * v for v in c['axisN'])
        dun = sum(a * b for a, b in zip(c['axisU'], c['axisN']))
        if abs(lu - 1) > 1e-4 or abs(ln - 1) > 1e-4 or abs(dun) > 1e-4:
            raise Refusal('chart %d: axes are not orthonormal' % ch)
        if c['firstSlot'] != nextS:
            raise Refusal('chart %d: first slot %d, but the charts before it end at %d' % (ch, c['firstSlot'], nextS))
        n = c['nu'] * c['nv'] * c['nw']
        if c['firstSlot'] + n > len(SL):
            raise Refusal('chart %d: slot range runs past %d slots' % (ch, len(SL)))
        held = 0
        for k in range(n):
            p = SL[c['firstSlot'] + k]
            if p == NONE:
                continue
            if p >= np_:
                raise Refusal('chart %d slot %d: patch %d is past %d patches' % (ch, k, p, np_))
            if P[p]['lodSource'] != ch or P[p]['kind'] == K_TERRAIN:
                raise Refusal('chart %d slot %d: patch %d belongs to chart %d, not this one'
                              % (ch, k, p, P[p]['lodSource']))
            refs[p] += 1
            held += 1
        if not held:
            raise Refusal('chart %d: no slot holds a patch' % ch)
        fb = c['fallbackPatch']
        if fb >= np_ or P[fb]['lodSource'] != ch or P[fb]['kind'] == K_TERRAIN:
            raise Refusal('chart %d: fallback patch %d is not one of its own' % (ch, fb))
        for k in range(n):
            p = SL[c['firstSlot'] + k]
            if p != NONE and (P[p]['kind'] == K_TREE) != (c['kind'] == 1):
                raise Refusal('chart %d: patch %d is kind %d on a %s chart'
                              % (ch, p, P[p]['kind'], 'tree' if c['kind'] == 1 else 'surface'))
        nextS += n
    if nextS != len(SL):
        raise Refusal('the charts hold %d slots, the table %d' % (nextS, len(SL)))
    nl = len(S['links'])
    for i, q in enumerate(P):
        if q['reserved']:
            raise Refusal('patch %d: reserved word is %d, not 0' % (i, q['reserved']))
        if q['kind'] == K_WATER:
            raise Refusal('patch %d: kind water is reserved (section 1: water takes no patches)' % i)
        if q['kind'] > K_TREE:
            raise Refusal('patch %d: kind %d is unknown' % (i, q['kind']))
        if q['flags'] & ~3:
            raise Refusal('patch %d: flags 0x%x carry unknown bits' % (i, q['flags']))
        if q['flags'] & SUBMERGED and q['kind'] != K_TERRAIN:
            raise Refusal('patch %d: only terrain is flagged submerged' % i)
        a = half_val(q['area'])
        if not (a > 0) or not math.isfinite(a):
            raise Refusal('patch %d: area %r is not positive' % (i, a))
        if q['firstLink'] + q['linkCount'] > nl:
            raise Refusal('patch %d: link range runs past %d links' % (i, nl))
        if q['kind'] != K_TERRAIN and refs[i] != 1:
            raise Refusal('patch %d: %d slots name it, not exactly one' % (i, refs[i]))


# ----------------------------------------------------------------- vectors (double, the C++ order)
def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def vlen(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


# ----------------------------------------------------------------- the chart rule
def mesh_tris(L, mi):
    m = L['meshes'][mi]
    out = []
    for k in range(m['clusterCount']):
        ci = m['clusterFirst'] + k
        c = L['clusters'][ci]
        level = L['clusterLods'][ci]['level'] if ci < len(L['clusterLods']) else 0
        mat = L['materials'][c['materialId']] if c['materialId'] < len(L['materials']) else None
        fol = bool(mat and mat['flags'] & MAT_TREE and mat['alphaThreshold'] != 0)
        li = L['localIndices'][ci * 48:(ci + 1) * 48]
        for t in range(c['triangleCount']):
            p = []
            for e in range(3):
                v = L['vertices'][c['vertexBase'] + li[3 * t + e]]
                p.append(tuple(m['aabbMin'][a] + v[('px', 'py', 'pz')[a]] / 65535.0 * m['aabbExtent'][a]
                               for a in range(3)))
            cr = cross(sub(p[1], p[0]), sub(p[2], p[0]))
            out.append({'slot': k * 16 + t, 'level': level, 'foliage': fol, 'p': p, 'cr': cr, 'crLen': vlen(cr)})
    return out


def mesh_charts(L, mi):
    m = L['meshes'][mi]
    tris = mesh_tris(L, mi)
    tri_chart = [CNONE] * (m['clusterCount'] * 16)
    chart = [-1] * len(tris)
    n = [mul(t['cr'], 1.0 / t['crLen']) if t['crLen'] > DEGENERATE else (0.0, 0.0, 0.0) for t in tris]

    def chartable(t):
        return tris[t]['level'] == 0 and not tris[t]['foliage'] and tris[t]['crLen'] > DEGENERATE
    nc = 0
    for t in range(len(tris)):
        if not chartable(t) or chart[t] >= 0:
            continue
        for u in range(t, len(tris)):
            if chartable(u) and chart[u] < 0 and dot(n[u], n[t]) >= CREASE_COS - CREASE_TIE:
                chart[u] = nc
        nc += 1
    if nc > CHART_MAX + 1:
        raise Refusal('mesh %d: %d charts' % (mi, nc))
    sums = [(0.0, 0.0, 0.0)] * nc
    for t in range(len(tris)):
        if chart[t] >= 0:
            sums[chart[t]] = add(sums[chart[t]], tris[t]['cr'])
    normal = []
    for c in range(nc):
        ln = vlen(sums[c])
        normal.append(mul(sums[c], 1.0 / ln) if ln > 0 else (0.0, 0.0, 1.0))
    foliage = False
    for t, tr in enumerate(tris):
        cid = CNONE
        if tr['foliage']:
            cid = FOLIAGE
            if tr['level'] == 0 and tr['crLen'] > DEGENERATE:
                foliage = True
        elif tr['crLen'] > DEGENERATE:
            if chart[t] >= 0:
                cid = chart[t]
            elif nc > 0:
                best, bd = 0, -2.0
                for c in range(nc):
                    d = n[t][0] * normal[c][0] + n[t][1] * normal[c][1] + n[t][2] * normal[c][2]
                    if d > bd:
                        bd, best = d, c
                cid = best
        tri_chart[tr['slot']] = cid
    return {'charts': nc, 'foliage': foliage, 'triChart': tri_chart, 'normal': normal, 'tris': tris}


def chart_stream(L):
    out = bytearray([CNONE]) * (len(L['clusters']) * 16)
    per = []
    for mi, m in enumerate(L['meshes']):
        mc = mesh_charts(L, mi)
        out[m['clusterFirst'] * 16:m['clusterFirst'] * 16 + len(mc['triChart'])] = bytes(mc['triChart'])
        per.append(mc)
    return bytes(out), per


# ----------------------------------------------------------------- the slot rule (placement twin)
def frame_of(mc):
    nc = mc['charts']
    fr = {'origin': [], 'axisU': [], 'axisN': []}
    for c in range(nc):
        N = mc['normal'][c]
        up = (0.0, 0.0, 1.0) if abs(N[2]) < 0.9 else (1.0, 0.0, 0.0)
        U = cross(up, N)
        U = mul(U, 1.0 / vlen(U))
        V = cross(N, U)
        lo = [1e300, 1e300, 1e300]
        for t in mc['tris']:
            if t['level'] != 0 or mc['triChart'][t['slot']] != c:
                continue
            for p in t['p']:
                lo[0] = min(lo[0], dot(p, U))
                lo[1] = min(lo[1], dot(p, V))
                lo[2] = min(lo[2], dot(p, N))
        o = add(add(mul(U, lo[0]), mul(V, lo[1])), mul(N, lo[2]))
        fr['origin'].append(tuple(f32(x) for x in o))
        fr['axisU'].append(tuple(f32(x) for x in U))
        fr['axisN'].append(tuple(f32(x) for x in N))
    fmin, fmax, farea = [1e300] * 3, [-1e300] * 3, 0.0
    for t in mc['tris']:
        if t['level'] != 0 or not t['foliage'] or not t['crLen'] > DEGENERATE:
            continue
        farea += 0.5 * t['crLen']
        for p in t['p']:
            fmin = [min(fmin[a], p[a]) for a in range(3)]
            fmax = [max(fmax[a], p[a]) for a in range(3)]
    fr['folMin'], fr['folMax'], fr['folArea'] = tuple(fmin), tuple(fmax), farea
    return fr


def clip_plane(poly, o, a, bound, keep_greater):
    out = []
    n = len(poly)
    for k in range(n):
        p, q = poly[k], poly[(k + 1) % n]
        fp = dot(sub(p, o), a) - bound
        fq = dot(sub(q, o), a) - bound
        if not keep_greater:
            fp, fq = -fp, -fq
        if fp >= 0:
            out.append(p)
        if (fp >= 0) != (fq >= 0):
            t = fp / (fp - fq)
            out.append(add(p, mul(sub(q, p), t)))
    return out


def poly_area(p):
    a = 0.0
    c = (0.0, 0.0, 0.0)
    for k in range(1, len(p) - 1):
        t = 0.5 * vlen(cross(sub(p[k], p[0]), sub(p[k + 1], p[0])))
        a += t
        c = add(c, mul(add(add(p[0], p[k]), p[k + 1]), t / 3.0))
    return a, c


def closest_on_seg(p, a, b):
    ab = sub(b, a)
    l2 = dot(ab, ab)
    t = min(1.0, max(0.0, dot(sub(p, a), ab) / l2)) if l2 > 0 else 0.0
    return add(a, mul(ab, t))


def closest_on_poly(p, poly, nrm):
    q = sub(p, mul(nrm, dot(sub(p, poly[0]), nrm)))
    n = len(poly)
    inside = True
    for k in range(n):
        e = sub(poly[(k + 1) % n], poly[k])
        if dot(cross(e, sub(q, poly[k])), nrm) < -1e-12:
            inside = False
            break
    if inside:
        return q
    best, bd = poly[0], sys.float_info.max
    for k in range(n):
        c = closest_on_seg(p, poly[k], poly[(k + 1) % n])
        d = dot(sub(c, p), sub(c, p))
        if d < bd:
            bd, best = d, c
    return best


def slot_mesh(mc, fr, step, red=0):
    out = []
    s = step
    for c in range(mc['charts']):
        O, U, N = fr['origin'][c], fr['axisU'][c], fr['axisN'][c]
        V = cross(N, U)
        ax = (U, V, N)
        mine = [t for t in mc['tris'] if t['level'] == 0 and mc['triChart'][t['slot']] == c]
        if red == 2 and len(mine) > 1:
            mine.pop()
        nmax = [0, 0, 0]
        for t in mine:
            for p in t['p']:
                for a in range(3):
                    nmax[a] = max(nmax[a], int(math.ceil(dot(sub(p, O), ax[a]) / s)) - 1)
        nu, nv, nw = nmax[0] + 1, nmax[1] + 1, nmax[2] + 1
        nn = (nu, nv, nw)
        pieces = {}
        for t in mine:
            lo, hi = [0] * 3, [0] * 3
            for a in range(3):
                ds = [dot(sub(p, O), ax[a]) for p in t['p']]
                lo[a] = min(nn[a] - 1, max(0, int(math.floor(min(ds) / s))))
                hi[a] = min(nn[a] - 1, max(0, int(math.floor(max(ds) / s))))
            tn = mul(t['cr'], 1.0 / t['crLen'])
            tri = list(t['p'])
            for i in range(lo[0], hi[0] + 1):
                pi = tri
                if i > 0:
                    pi = clip_plane(pi, O, U, i * s, True)
                if i < nu - 1:
                    pi = clip_plane(pi, O, U, (i + 1) * s, False)
                if len(pi) < 3:
                    continue
                for j in range(lo[1], hi[1] + 1):
                    pj = pi
                    if j > 0:
                        pj = clip_plane(pj, O, V, j * s, True)
                    if j < nv - 1:
                        pj = clip_plane(pj, O, V, (j + 1) * s, False)
                    if len(pj) < 3:
                        continue
                    for k in range(lo[2], hi[2] + 1):
                        pk = pj
                        if k > 0:
                            pk = clip_plane(pk, O, N, k * s, True)
                        if k < nw - 1:
                            pk = clip_plane(pk, O, N, (k + 1) * s, False)
                        if len(pk) < 3:
                            continue
                        pieces.setdefault((k * nv + j) * nu + i, []).append((pk, tn))
        total = nu * nv * nw
        slots = [NONE] * total
        patches = []
        fallback = 0
        min_area = 1e-6 * s * s
        best_a = -1.0
        for idx in range(total):
            if idx not in pieces:
                continue
            area = 0.0
            cs = ns = (0.0, 0.0, 0.0)
            for poly, tn in pieces[idx]:
                a, ca = poly_area(poly)
                area += a
                cs = add(cs, ca)
                ns = add(ns, mul(tn, a))
            if not area > min_area:
                continue
            cen = mul(cs, 1.0 / area)
            best, bd = cen, sys.float_info.max
            for poly, tn in pieces[idx]:
                if poly_area(poly)[0] <= 0:
                    continue
                q = closest_on_poly(cen, poly, tn)
                d = dot(sub(q, cen), sub(q, cen))
                if d < bd:
                    bd, best = d, q
            nl = vlen(ns)
            lp = {'pos': best, 'nrm': mul(ns, 1.0 / nl) if nl > 0 else N, 'area': area,
                  'moved': math.sqrt(bd) > 1e-4 * s}
            slots[idx] = len(patches)
            if area > best_a:
                best_a, fallback = area, len(patches)
            patches.append(lp)
        if not patches:
            pos = mine[0]['p'][0] if mine else O
            a = sum(0.5 * t['crLen'] for t in mine)
            idx = 0
            if mine:
                ijk = [min(nn[q] - 1, max(0, int(math.floor(dot(sub(pos, O), ax[q]) / s)))) for q in range(3)]
                idx = (ijk[2] * nv + ijk[1]) * nu + ijk[0]
            slots[idx] = 0
            fallback = 0
            patches.append({'pos': pos, 'nrm': N, 'area': max(a, min_area), 'moved': False})
        out.append({'id': c, 'kind': 0, 'nu': nu, 'nv': nv, 'nw': nw, 'step': step, 'origin': O, 'axisU': U,
                    'axisN': N, 'slots': slots, 'patches': patches, 'fallback': fallback})
    if mc['foliage']:
        fmin, fmax = fr['folMin'], fr['folMax']
        out.append({'id': FOLIAGE, 'kind': 1, 'nu': 1, 'nv': 1, 'nw': 1, 'step': step,
                    'origin': tuple(f32(x) for x in fmin), 'axisU': (1.0, 0.0, 0.0), 'axisN': (0.0, 0.0, 1.0),
                    'slots': [0], 'fallback': 0,
                    'patches': [{'pos': mul(add(fmin, fmax), 0.5), 'nrm': (0.0, 0.0, 1.0), 'area': fr['folArea'],
                                 'moved': False}]})
    return out


def pack_oct8(n):
    f = np.float32
    x0, y0, z0 = f(n[0]), f(n[1]), f(n[2])
    s = abs(x0) + abs(y0) + abs(z0)
    if not s > f(0):
        s = f(1)
    x, y = x0 / s, y0 / s
    if z0 < f(0):
        ox = (f(1) - abs(y)) * (f(1) if x >= f(0) else f(-1))
        oy = (f(1) - abs(x)) * (f(1) if y >= f(0) else f(-1))
        x, y = ox, oy

    def q(v):
        w = float((min(f(1), max(f(-1), v)) * f(0.5) + f(0.5)) * f(255))
        i = int(math.floor(abs(w) + 0.5)) * (1 if w >= 0 else -1)
        return min(255, max(0, i))
    return q(x) | (q(y) << 8)


def unpack_oct8(h):
    x = (h & 0xFF) / 255.0 * 2 - 1
    y = (h >> 8) / 255.0 * 2 - 1
    z = 1 - abs(x) - abs(y)
    if z < 0:
        x, y = (1 - abs(y)) * (1 if x >= 0 else -1), (1 - abs(x)) * (1 if y >= 0 else -1)
    ln = math.sqrt(x * x + y * y + z * z)
    return (x / ln, y / ln, z / ln)


def llround(x):
    return int(math.floor(abs(x) + 0.5)) * (1 if x >= 0 else -1)


def mul_r(r, v):
    return (r[0] * v[0] + r[1] * v[1] + r[2] * v[2], r[3] * v[0] + r[4] * v[1] + r[5] * v[2],
            r[6] * v[0] + r[7] * v[1] + r[8] * v[2])


def blank_patch():
    return {'pos': (0, 0, 0), 'normal': 0, 'albedo': (0, 0, 0), 'flags': 0, 'area': 0, 'sampleCount': 0, 'kind': 0,
            'horizon': bytes(16), 'openness': bytes(16), 'firstLink': 0, 'linkCount': 0, 'push': 0,
            'lodSource': 0, 'reserved': 0}


def place(L, placements, terrain, opt):
    """The twin placement. placements: dicts (lodiIndex, mesh, pos, rot, scale) of float32 values;
    terrain: None or dict(minX, minY, maxX, maxY, spc, height(gx, gy), cell(cx, cy) -> (land, water, wh))."""
    G = opt['grid']
    pu = opt['patchUnits']
    red = opt.get('red', 0)
    charts_of, slotted, cells = {}, {}, {}
    for pl in placements:
        if pl['mesh'] not in charts_of:
            mc = mesh_charts(L, pl['mesh'])
            charts_of[pl['mesh']] = (mc, frame_of(mc))
        step = f32(pu / pl['scale'])
        key = (pl['mesh'], fbits(step))
        if key not in slotted:
            mc, fr = charts_of[pl['mesh']]
            slotted[key] = slot_mesh(mc, fr, step, red)
        if not slotted[key]:
            continue
        cx = int(math.floor(pl['pos'][0] / CELL_UNITS))
        cy = int(math.floor(pl['pos'][1] / CELL_UNITS))
        cb = cells.setdefault((cx, cy), {'terrain': False, 'terr': [], 'insts': []})
        cb['insts'].append((pl, slotted[key]))
    if terrain and terrain['spc'] > 0:
        spc = terrain['spc']
        stp = spc // G
        d = CELL_UNITS / spc
        lastX = (terrain['maxX'] - terrain['minX'] + 1) * spc - 1
        lastY = (terrain['maxY'] - terrain['minY'] + 1) * spc - 1

        def H(gx, gy):
            return float(terrain['height'](min(gx, lastX), min(gy, lastY)))
        for cy in range(terrain['minY'], terrain['maxY'] + 1):
            for cx in range(terrain['minX'], terrain['maxX'] + 1):
                land, water, wh = terrain['cell'](cx, cy)
                if not land:
                    continue
                cb = cells.setdefault((cx, cy), {'terrain': False, 'terr': [], 'insts': []})
                cb['terrain'] = True
                bx, by = (cx - terrain['minX']) * spc, (cy - terrain['minY']) * spc
                for j in range(G):
                    for i in range(G):
                        cr = (0.0, 0.0, 0.0)
                        area = 0.0
                        for v in range(stp):
                            for u in range(stp):
                                gx, gy = bx + i * stp + u, by + j * stp + v
                                p00, p10 = (0.0, 0.0, H(gx, gy)), (d, 0.0, H(gx + 1, gy))
                                p01, p11 = (0.0, d, H(gx, gy + 1)), (d, d, H(gx + 1, gy + 1))
                                c1 = cross(sub(p10, p00), sub(p11, p00))
                                c2 = cross(sub(p11, p00), sub(p01, p00))
                                cr = add(add(cr, c1), c2)
                                area += 0.5 * (vlen(c1) + vlen(c2))
                        z = H(bx + i * stp + stp // 2, by + j * stp + stp // 2)
                        cl = vlen(cr)
                        p = blank_patch()
                        p['normal'] = pack_oct8((f32(cr[0] / cl), f32(cr[1] / cl), f32(cr[2] / cl)))
                        p['area'] = half_bits(f32(area / AREA_UNIT))
                        p['kind'] = K_TERRAIN
                        p['lodSource'] = j * G + i
                        if water and z < wh:
                            p['flags'] |= SUBMERGED
                        cb['terr'].append((p, cx * CELL_UNITS + (i * stp + stp // 2) * d,
                                           cy * CELL_UNITS + (j * stp + stp // 2) * d, z))
    by_sector = {}
    for (cx, cy) in sorted(cells):
        by_sector.setdefault((sector_of(cx), sector_of(cy)), []).append((cx, cy))
    out = {}
    for (sx, sy) in sorted(by_sector):
        S = {'flags': 1 if opt['bound'] else 0, 'worldspaceHash': opt['worldspaceHash'],
             'lodInputHash': opt['lodInputHash'], 'lodoIdentity': opt['lodoIdentity'],
             'optionsHash': options_hash(pu, G), 'sectorX': sx, 'sectorY': sy, 'patchUnits': pu, 'terrainGrid': G,
             'cells': [], 'instances': [], 'charts': [], 'slots': [], 'patches': [], 'links': []}
        by_index = {}
        for (cx, cy) in by_sector[(sx, sy)]:
            by_index[(cy - sy * 16) * 16 + (cx - sx * 16)] = (cx, cy)
        rows, chart_row = [], []
        for ci in range(NCELLS):
            cell = {'firstPatch': len(S['patches']), 'patchCount': 0, 'firstChart': len(S['charts']), 'chartCount': 0,
                    'inputHash': 0, 'zBase': 0.0, 'flags': 0, 'reserved': 0}
            S['cells'].append(cell)
            if ci not in by_index:
                continue
            cx, cy = by_index[ci]
            cb = cells[(cx, cy)]
            pend = []
            if cb['terrain']:
                cell['flags'] |= CELL_TERRAIN
                pend.extend(cb['terr'])
            for pl, lcs in sorted(cb['insts'], key=lambda e: e[0]['lodiIndex']):
                row = {'lodiIndex': pl['lodiIndex'], 'firstChart': len(S['charts']), 'chartCount': len(lcs),
                       'lodoMesh': pl['mesh']}
                T = pl['pos']
                sc = pl['scale']
                for lc in lcs:
                    chart_index = len(S['charts'])
                    base = len(pend)
                    c = {'firstSlot': len(S['slots']), 'nu': lc['nu'], 'nv': lc['nv'], 'nw': lc['nw'],
                         'meshChart': lc['id'], 'kind': lc['kind'], 'step': lc['step'],
                         'origin': tuple(f32(x) for x in lc['origin']), 'axisU': tuple(f32(x) for x in lc['axisU']),
                         'axisN': tuple(f32(x) for x in lc['axisN']), 'instance': len(rows),
                         'fallbackPatch': base + lc['fallback'], 'reserved': 0}
                    S['slots'].extend(NONE if v == NONE else base + v for v in lc['slots'])
                    S['charts'].append(c)
                    chart_row.append(len(rows))
                    for lp in lc['patches']:
                        p = blank_patch()
                        w = add(mul_r(pl['rot'], mul(lp['pos'], sc)), T)
                        if lc['kind'] == 1:
                            wn = (0.0, 0.0, 1.0)
                        else:
                            wn = mul_r(pl['rot'], lp['nrm'])
                            wn = mul(wn, 1.0 / vlen(wn))
                        if red == 1 and lc['kind'] != 1:
                            w = add(w, mul(wn, 8.0))
                        p['normal'] = pack_oct8(tuple(f32(x) for x in wn))
                        p['area'] = half_bits(f32(lp['area'] * sc * sc / AREA_UNIT))
                        if not half_val(p['area']) > 0:
                            p['area'] = 1
                        p['kind'] = K_TREE if lc['kind'] == 1 else K_OBJECT
                        p['lodSource'] = chart_index
                        pend.append((p, w[0], w[1], w[2]))
                rows.append(row)
            zs = [q[3] for q in pend]
            cell['zBase'] = f32(math.floor((min(zs) + max(zs)) * 0.5)) if pend else 0.0
            for p, x, y, z in pend:
                rel = (x - cx * CELL_UNITS, y - cy * CELL_UNITS, z - cell['zBase'])
                p['pos'] = tuple(llround(r / POS_STEP) for r in rel)
                if not all(-32768 <= v <= 32767 for v in p['pos']):
                    raise Refusal('cell (%d, %d): a patch past the i16 position' % (cx, cy))
                S['patches'].append(p)
            cell['patchCount'] = len(S['patches']) - cell['firstPatch']
            cell['chartCount'] = len(S['charts']) - cell['firstChart']
            for ch in range(cell['firstChart'], cell['firstChart'] + cell['chartCount']):
                c = S['charts'][ch]
                c['fallbackPatch'] += cell['firstPatch']
                for k in range(c['firstSlot'], c['firstSlot'] + c['nu'] * c['nv'] * c['nw']):
                    if S['slots'][k] != NONE:
                        S['slots'][k] += cell['firstPatch']
        order = sorted(range(len(rows)), key=lambda k: rows[k]['lodiIndex'])
        row_at = [0] * len(rows)
        for k, o in enumerate(order):
            row_at[o] = k
            r = rows[o]
            S['instances'].append({'lodiIndex': r['lodiIndex'], 'firstChart': r['firstChart'],
                                   'chartCount': r['chartCount'], 'reserved': 0, 'lodoMesh': r['lodoMesh']})
        for ch, c in enumerate(S['charts']):
            c['instance'] = row_at[chart_row[ch]]
        if not S['patches']:
            continue
        out[(sx, sy)] = S
    return out


def serialise(S):
    ni, nc, ns, np_, nl = (len(S['instances']), len(S['charts']), len(S['slots']), len(S['patches']), len(S['links']))
    L = layout(ni, nc, ns, np_, nl)
    t = bytearray()
    for c in S['cells']:
        t += CELL.pack(c['firstPatch'], c['patchCount'], c['firstChart'], c['chartCount'], c['inputHash'], c['zBase'],
                       c['flags'], c['reserved'])
    for r in S['instances']:
        t += INST.pack(r['lodiIndex'], r['firstChart'], r['chartCount'], r['reserved'], r['lodoMesh'])
    for c in S['charts']:
        t += CHART.pack(c['firstSlot'], c['nu'], c['nv'], c['nw'], c['meshChart'], c['kind'], c['step'],
                        *c['origin'], *c['axisU'], *c['axisN'], c['instance'], c['fallbackPatch'], c['reserved'])
    t += struct.pack('<%dI' % ns, *S['slots'])
    for p in S['patches']:
        t += PATCH.pack(*p['pos'], p['normal'], *p['albedo'], p['flags'], p['area'], p['sampleCount'], p['kind'],
                        p['horizon'], p['openness'], p['firstLink'], p['linkCount'], p['push'], p['lodSource'],
                        p['reserved'])
    for k in S['links']:
        t += LINK.pack(*k)
    h = bytearray(HDR)
    struct.pack_into('<III', h, 0, MAGIC, VERSION, S['flags'])
    struct.pack_into('<QQQQ', h, 0x10, S['worldspaceHash'], S['lodInputHash'], S['lodoIdentity'], S['optionsHash'])
    struct.pack_into('<iifHB', h, 0x30, S['sectorX'], S['sectorY'], S['patchUnits'], 16, S['terrainGrid'])
    struct.pack_into('<7I', h, 0x40, NCELLS, ni, nc, ns, np_, nl, crc32(bytes(t)))
    struct.pack_into('<7Q', h, 0x60, L['cell'], L['inst'], L['chart'], L['slot'], L['patch'], L['link'], L['end'])
    struct.pack_into('<I', h, 0x0C, crc32(bytes(h[0x10:])))
    return bytes(h) + bytes(t)


# ----------------------------------------------------------------- the fixture gates
def fixture_height(gx, gy):
    return float(64 + 3 * (gx % 37) + 2 * (gy % 23))


FIXTURE_TERRAIN = {
    'minX': -1, 'maxX': 16, 'minY': -1, 'maxY': 0, 'spc': 16, 'height': fixture_height,
    'cell': lambda cx, cy: (not (cx == 16 and cy == 0), (cx, cy) in ((0, 0), (-1, 0)),
                            1000.0 if (cx, cy) == (0, 0) else 150.0)}


def read_placements(path):
    out = []
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        v = line.split('\t')
        fl = [struct.unpack('<f', struct.pack('<I', int(x, 16)))[0] for x in v[2:15]]
        out.append({'lodiIndex': int(v[0]), 'mesh': int(v[1]), 'pos': tuple(fl[0:3]), 'rot': tuple(fl[3:12]),
                    'scale': fl[12]})
    return out


class Gates:
    def __init__(self):
        self.n = 0
        self.fails = {}

    def check(self, gate, ok, msg):
        self.n += 1
        if not ok:
            self.fails.setdefault(gate, []).append(msg)
        return ok


def point_tri_dist(p, a, b, c):
    """Exact distance from p to triangle abc (Ericson's closest point)."""
    ab, ac, ap = sub(b, a), sub(c, a), sub(p, a)
    d1, d2 = dot(ab, ap), dot(ac, ap)
    if d1 <= 0 and d2 <= 0:
        q = a
    else:
        bp = sub(p, b)
        d3, d4 = dot(ab, bp), dot(ac, bp)
        cp = sub(p, c)
        d5, d6 = dot(ab, cp), dot(ac, cp)
        vc = d1 * d4 - d3 * d2
        vb = d5 * d2 - d1 * d6
        va = d3 * d6 - d5 * d4
        if d3 >= 0 and d4 <= d3:
            q = b
        elif vc <= 0 and d1 >= 0 and d3 <= 0:
            q = add(a, mul(ab, d1 / (d1 - d3)))
        elif d6 >= 0 and d5 <= d6:
            q = c
        elif vb <= 0 and d2 >= 0 and d6 <= 0:
            q = add(a, mul(ac, d2 / (d2 - d6)))
        elif va <= 0 and (d4 - d3) >= 0 and (d5 - d6) >= 0:
            q = add(b, mul(sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))))
        else:
            den = 1.0 / (va + vb + vc)
            q = add(a, add(mul(ab, vb * den), mul(ac, vc * den)))
    return vlen(sub(p, q))


def patch_world(S, ci, p):
    cx = S['sectorX'] * 16 + ci % 16
    cy = S['sectorY'] * 16 + ci // 16
    return (cx * CELL_UNITS + p['pos'][0] * POS_STEP, cy * CELL_UNITS + p['pos'][1] * POS_STEP,
            S['cells'][ci]['zBase'] + p['pos'][2] * POS_STEP)


def eval_expect(s):
    return eval(s, {'__builtins__': {}}, {})


def instance_gates(g, S, sxy, inst, pl, mc, worst, patches_of_inst, area_of_inst, tree_area):
    """Gates F, B and A on one instance row: every chart's patches on its own faces, inside
    their slots, and summing to its triangles' area. `pl` = {rot, pos, scale}."""
    sx, sy = sxy
    R, T, sc = pl['rot'], pl['pos'], pl['scale']
    for k in range(inst['chartCount']):
        ch = inst['firstChart'] + k
        c = S['charts'][ch]
        ci = next(q for q, cl in enumerate(S['cells'])
                  if cl['firstChart'] <= ch < cl['firstChart'] + cl['chartCount'])
        if c['kind'] == 1:
            tris = [t for t in mc['tris'] if t['level'] == 0 and t['foliage'] and t['crLen'] > DEGENERATE]
        else:
            tris = [t for t in mc['tris'] if t['level'] == 0 and mc['triChart'][t['slot']] == c['meshChart']]
        want_area = sum(0.5 * t['crLen'] for t in tris) * sc * sc
        got_area = 0.0
        n = c['nu'] * c['nv'] * c['nw']
        for sk in range(n):
            pi = S['slots'][c['firstSlot'] + sk]
            if pi == NONE:
                continue
            p = S['patches'][pi]
            a = half_val(p['area']) * AREA_UNIT
            got_area += a
            patches_of_inst[inst['lodiIndex']] = patches_of_inst.get(inst['lodiIndex'], 0) + 1
            if c['kind'] == 1:
                tree_area[inst['lodiIndex']] = a
                continue
            area_of_inst[inst['lodiIndex']] = area_of_inst.get(inst['lodiIndex'], 0.0) + a
            w = patch_world(S, ci, p)
            # back to mesh-local: R^T (w - T) / scale
            dw = sub(w, T)
            loc = tuple((R[0 + a_] * dw[0] + R[3 + a_] * dw[1] + R[6 + a_] * dw[2]) / sc for a_ in range(3))
            # F: on a face of its own chart (world units)
            dist = min(point_tri_dist(loc, *t['p']) for t in tris) * sc
            worst['F'] = max(worst['F'], dist)
            g.check('F', dist <= 0.45, 'sector %s chart %d patch %d is %.3f u off its faces'
                    % ((sx, sy), ch, pi, dist))
            # B: inside the slot that names it
            O, U, N = c['origin'], c['axisU'], c['axisN']
            V = cross(N, U)
            ijk = (sk % c['nu'], (sk // c['nu']) % c['nv'], sk // (c['nu'] * c['nv']))
            tol = 0.45 / sc
            out_by = 0.0
            for a_, axv in enumerate((U, V, N)):
                dd = dot(sub(loc, O), axv)
                lo = ijk[a_] * c['step'] if ijk[a_] > 0 else -1e300
                hi = (ijk[a_] + 1) * c['step'] if ijk[a_] < (c['nu'], c['nv'], c['nw'])[a_] - 1 else 1e300
                out_by = max(out_by, lo - dd, dd - hi)
            worst['B'] = max(worst['B'], out_by * sc)
            g.check('B', out_by <= tol, 'sector %s chart %d slot %d: patch %d stands %.3f u outside it'
                    % ((sx, sy), ch, sk, pi, out_by * sc))
        rel = abs(got_area - want_area) / want_area if want_area > 0 else 0.0
        worst['A'] = max(worst['A'], rel)
        g.check('A', rel <= 2e-3, 'sector %s chart %d: patch areas %.1f, its triangles %.1f (rel %.4f)'
                % ((sx, sy), ch, got_area, want_area, rel))


def run_fixture(d, expect_red=None):
    g = Gates()
    L = nd.read_lodo(os.path.join(d, 'Synthetic.lodo'))
    stream, per = chart_stream(L)
    # S: the stream
    g.check('S', L['chartStream'] is not None, 'the .lodo carries no chart stream')
    if L['chartStream'] is not None:
        diff = sum(1 for a, b in zip(stream, L['chartStream']) if a != b)
        g.check('S', diff == 0 and len(stream) == len(L['chartStream']),
                'chart stream: %d of %d bytes differ from the recompute' % (diff, len(stream)))
    pls = read_placements(os.path.join(d, 'placements.tsv'))
    files = sorted(f for f in os.listdir(d) if f.startswith('lodp_') and f.endswith('.lodp'))
    sectors = {}
    raw = {}
    for f in files:
        b = open(os.path.join(d, f), 'rb').read()
        try:
            S = read_lodp(b)
        except Refusal as e:
            g.check('R', False, '%s REFUSED: %s' % (f, e))
            continue
        g.check('R', f == 'lodp_%d_%d.lodp' % (S['sectorX'], S['sectorY']), '%s names another sector' % f)
        sectors[(S['sectorX'], S['sectorY'])] = S
        raw[(S['sectorX'], S['sectorY'])] = b
    lh = L['header']
    opt = {'grid': 8, 'patchUnits': 512.0, 'bound': True, 'worldspaceHash': 0x5555555555555555,
           'lodInputHash': 0x6666666666666666, 'lodoIdentity': nd.lodo_identity(lh), 'red': 0}
    # T: the twin's bytes (the GREEN placement: a red file must differ)
    twin = place(L, pls, FIXTURE_TERRAIN, opt)
    g.check('T', sorted(twin) == sorted(sectors), 'twin sectors %s, files %s' % (sorted(twin), sorted(sectors)))
    for k in sorted(twin):
        if k not in raw:
            continue
        tb = serialise(twin[k])
        if tb != raw[k]:
            nd_ = sum(1 for a, b in zip(tb, raw[k]) if a != b)
            first = next((i for i, (a, b) in enumerate(zip(tb, raw[k])) if a != b), min(len(tb), len(raw[k])))
            g.check('T', False, 'sector %s: %d bytes vs %d, %d differ, first at 0x%x'
                    % (k, len(tb), len(raw[k]), nd_, first))
        else:
            g.check('T', True, '')
    # geometric gates, from the FILES
    by_lodi = {p['lodiIndex']: p for p in pls}
    meshes = {mi: per[mi] for mi in range(len(per))}
    worst = {'F': 0.0, 'A': 0.0, 'B': 0.0, 'Z': 0.0}
    patches_of_inst, area_of_inst, tree_area = {}, {}, {}
    land_cells = terr_patches = 0
    sub00 = 0
    for (sx, sy), S in sorted(sectors.items()):
        for ci, cell in enumerate(S['cells']):
            if cell['flags'] & CELL_TERRAIN:
                land_cells += 1
            cx, cy = sx * 16 + ci % 16, sy * 16 + ci // 16
            for k in range(cell['patchCount']):
                p = S['patches'][cell['firstPatch'] + k]
                if p['kind'] != K_TERRAIN:
                    continue
                terr_patches += 1
                if (cx, cy) == (0, 0) and p['flags'] & SUBMERGED:
                    sub00 += 1
                # Z: the ground's centre sample
                i, j = p['lodSource'] % 8, p['lodSource'] // 8
                gx = (cx - FIXTURE_TERRAIN['minX']) * 16 + i * 2 + 1
                gy = (cy - FIXTURE_TERRAIN['minY']) * 16 + j * 2 + 1
                want = (cx * CELL_UNITS + (i * 2 + 1) * 256.0, cy * CELL_UNITS + (j * 2 + 1) * 256.0,
                        fixture_height(gx, gy))
                got = patch_world(S, ci, p)
                e = max(abs(a - b) for a, b in zip(got, want))
                worst['Z'] = max(worst['Z'], e)
                g.check('Z', e <= 0.25, 'terrain patch at cell (%d, %d) grid %d: off by %.3f u' % (cx, cy, p['lodSource'], e))
        for ii, inst in enumerate(S['instances']):
            pl = by_lodi.get(inst['lodiIndex'])
            if not g.check('I', pl is not None and pl['mesh'] == inst['lodoMesh'],
                           'instance %d names a placement the fixture did not make' % inst['lodiIndex']):
                continue
            instance_gates(g, S, (sx, sy), inst, pl, meshes[inst['lodoMesh']], worst,
                           patches_of_inst, area_of_inst, tree_area)
    # E: the expect file
    exp = {}
    for line in open(os.path.join(d, 'Synthetic.lodp.expect.txt')):
        parts = line.split(None, 2)
        if len(parts) == 3 and parts[0] == 'expect':
            exp[parts[1]] = parts[2].strip()
    for key, want in exp.items():
        if key == 'lodo.version':
            got = str(lh['version'])
        elif key.startswith('charts.mesh') and key.endswith('.coarse'):
            mi = int(key[len('charts.mesh'):-len('.coarse')])
            mc = meshes[mi]
            got = ','.join(sorted({str(mc['triChart'][t['slot']]) for t in mc['tris'] if t['level'] > 0}))
        elif key.startswith('charts.mesh'):
            mc = meshes[int(key[len('charts.mesh'):])]
            got = str(mc['charts'])
            if mc['foliage']:
                got += '+tree'
            nn = sum(1 for t in mc['tris'] if t['level'] == 0 and mc['triChart'][t['slot']] == CNONE)
            if nn:
                got += '+none%d' % nn
        elif key.startswith('patches.instance'):
            got = str(patches_of_inst.get(int(key[len('patches.instance'):]), 0))
        elif key.startswith('area.instance') and key.endswith('.tree'):
            v = tree_area.get(int(key[len('area.instance'):-len('.tree')]), 0.0)
            w = eval_expect(want)
            g.check('E', abs(v - w) <= 2e-3 * w, 'expect %s: %.1f, want %.1f' % (key, v, w))
            continue
        elif key.startswith('area.instance'):
            v = area_of_inst.get(int(key[len('area.instance'):]), 0.0)
            w = eval_expect(want)
            g.check('E', abs(v - w) <= 2e-3 * w, 'expect %s: %.1f, want %.1f' % (key, v, w))
            continue
        elif key == 'terrain.landCells':
            got = str(land_cells)
        elif key == 'terrain.patches':
            got = str(terr_patches)
        elif key == 'terrain.submerged.cell0_0':
            got = str(sub00)
        elif key == 'sectors':
            got = ','.join('%d_%d' % k for k in sorted(sectors))
        else:
            g.check('E', False, 'expect %s: this reader does not know the key' % key)
            continue
        g.check('E', got == want, 'expect %s: got %s, want %s' % (key, got, want))
    print('lodp-twin %s: %d sectors, %d checks; worst on-face %.3f u, area rel %.5f, slot %.3f u, terrain %.3f u'
          % (d, len(sectors), g.n, worst['F'], worst['A'], worst['B'], worst['Z']))
    for gate in sorted(g.fails):
        print('  gate %s: %d failures, first: %s' % (gate, len(g.fails[gate]), g.fails[gate][0]))
    if expect_red:
        gate = {'faces': 'F', 'area': 'A'}[expect_red]
        ok = gate in g.fails
        print('%s (red %s: gate %s %s)' % ('PASS' if ok else 'FAIL', expect_red, gate,
                                            'failed as it must' if ok else 'did NOT fail'))
        return 0 if ok else 1
    print('%d checks, %d failures' % (g.n, sum(len(v) for v in g.fails.values())))
    print('PASS' if not g.fails else 'FAIL')
    return 0 if not g.fails else 1


# ----------------------------------------------------------------- a real bake
def real_check(lodo_path, lodi_path, d, cap, region=None):
    """Gates R, I, P on every instance row of a real bake's sector files and F, B, A on `cap` of
    them, spread evenly. Placements come from the `.lodi` through lodgen_native_decode.py."""
    g = Gates()
    L = nd.read_lodo(lodo_path)
    T = nd.read_lodi(lodi_path)
    nmesh = len(L['meshes'])

    def first_rep(base_id):
        if base_id >= len(L['bases']):
            return None
        bse = L['bases'][base_id]
        for r in range(4):
            m = bse['rep%d' % r]
            if m != 0xFFFF and m < nmesh:
                return m
        return None

    files = sorted(f for f in os.listdir(d) if f.startswith('lodp_') and f.endswith('.lodp'))
    sectors = {}
    for f in files:
        try:
            S = read_lodp(open(os.path.join(d, f), 'rb').read())
        except Refusal as e:
            g.check('R', False, '%s REFUSED: %s' % (f, e))
            continue
        g.check('R', f == 'lodp_%d_%d.lodp' % (S['sectorX'], S['sectorY']), '%s names another sector' % f)
        sectors[(S['sectorX'], S['sectorY'])] = S
    g.check('R', len(sectors) > 0, 'no sector file in %s' % d)

    # the cells the files cover (a cell with any patch or chart), and the rows they hold
    covered = set()
    rows = []          # (sector key, instance row index, the cell the row's charts sit in)
    for key, S in sorted(sectors.items()):
        sx, sy = key
        for ci, cell in enumerate(S['cells']):
            if cell['patchCount'] or cell['chartCount']:
                covered.add((sx * 16 + ci % 16, sy * 16 + ci // 16))
        for ii, inst in enumerate(S['instances']):
            ch = inst['firstChart']
            ci = next(q for q, cl in enumerate(S['cells']) if cl['firstChart'] <= ch < cl['firstChart'] + cl['chartCount'])
            rows.append((key, ii, (sx * 16 + ci % 16, sy * 16 + ci // 16)))
    if region:
        x0, y0, x1, y1 = region
        covered = {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}

    insts = T['instances']
    mesh_cache = {}

    def charts_of(mi):
        if mi not in mesh_cache:
            mesh_cache[mi] = mesh_charts(L, mi)
        return mesh_cache[mi]

    def cell_of(r):
        return (int(math.floor(f32(r['x']) / CELL_UNITS)), int(math.floor(f32(r['y']) / CELL_UNITS)))

    # I: each row names a drawn placement, in the cell the placement stands in
    seen = set()
    for key, ii, cell in rows:
        inst = sectors[key]['instances'][ii]
        li = inst['lodiIndex']
        if not g.check('I', li < len(insts), 'row names .lodi instance %d past %d' % (li, len(insts))):
            continue
        r = insts[li]
        g.check('I', not (r['flags'] & 0x100), 'instance %d is Initially Disabled but has a row' % li)
        g.check('I', first_rep(r['baseId']) == inst['lodoMesh'],
                'instance %d: row mesh %d, the base\'s first present rep %s' % (li, inst['lodoMesh'], first_rep(r['baseId'])))
        g.check('I', cell_of(r) == cell, 'instance %d stands in cell %s, its charts sit in %s' % (li, cell_of(r), cell))
        g.check('I', li not in seen, 'instance %d has two rows' % li)
        seen.add(li)
        if ii:
            g.check('I', sectors[key]['instances'][ii - 1]['lodiIndex'] < li, 'instance rows not sorted at %d' % li)

    # P: every drawn placement in a covered cell whose mesh has charts has its row
    want = 0
    for li, r in enumerate(insts):
        if r['flags'] & 0x100 or cell_of(r) not in covered:
            continue
        mi = first_rep(r['baseId'])
        if mi is None:
            continue
        mc = charts_of(mi)
        if mc['charts'] == 0 and not mc['foliage']:
            continue
        want += 1
        g.check('P', li in seen, 'instance %d (cell %s, mesh %d, %d charts) has no row' % (li, cell_of(r), mi, mc['charts']))
    g.check('P', want == len(seen), 'placements with charts in the covered cells %d, rows %d' % (want, len(seen)))

    # F, B, A on a spread sample
    worst = {'F': 0.0, 'A': 0.0, 'B': 0.0, 'Z': 0.0}
    pick = rows if len(rows) <= cap else [rows[(k * len(rows)) // cap] for k in range(cap)]
    sampled_patches = 0
    for key, ii, cell in pick:
        S = sectors[key]
        inst = S['instances'][ii]
        r = insts[inst['lodiIndex']]
        pl = {'rot': r['m'], 'pos': (r['x'], r['y'], r['z']), 'scale': r['scaleF']}
        poi = {}
        instance_gates(g, S, key, inst, pl, charts_of(inst['lodoMesh']), worst, poi, {}, {})
        sampled_patches += sum(poi.values())
    print('lodp-real %s: %d sectors, %d instance rows, %d placements expected, %d meshes cut; '
          'sampled %d rows / %d patches: worst on-face %.3f u, area rel %.5f, slot %.3f u'
          % (d, len(sectors), len(rows), want, len(mesh_cache), len(pick), sampled_patches,
             worst['F'], worst['A'], worst['B']))
    for gate in sorted(g.fails):
        print('  gate %s: %d failures, first: %s' % (gate, len(g.fails[gate]), g.fails[gate][0]))
    print('%d checks, %d failures' % (g.n, sum(len(v) for v in g.fails.values())))
    print('PASS' if not g.fails else 'FAIL')
    return 0 if not g.fails else 1


# ----------------------------------------------------------------- tier-2 controls
def resign(b):
    b = bytearray(b)
    struct.pack_into('<I', b, 0x58, crc32(bytes(b[HDR:])))
    struct.pack_into('<I', b, 0x0C, crc32(bytes(b[0x10:HDR])))
    return bytes(b)


def controls(good, outdir):
    b = open(good, 'rb').read()
    S = read_lodp(b)
    L = layout(len(S['instances']), len(S['charts']), len(S['slots']), len(S['patches']), len(S['links']))
    os.makedirs(outdir, exist_ok=True)
    # a cell with terrain and charts, an object patch, a terrain patch
    ci = next(i for i, c in enumerate(S['cells']) if c['chartCount'] and c['flags'] & CELL_TERRAIN)
    obj = next(i for i, p in enumerate(S['patches']) if p['kind'] == K_OBJECT)
    terr = next(i for i, p in enumerate(S['patches']) if p['kind'] == K_TERRAIN)
    ch0 = 0
    ch_obj = S['patches'][obj]['lodSource']

    def u32(off, v):
        return ('<I', off, v)
    cases = [
        # (name, edits, substring the refusal must carry, resign?)
        ('t1_flip_payload', [('B', L['patch'] + 64 * obj + 40, None)], 'indexCrc32', False),
        ('t1_flip_header', [('B', 0x38, None)], 'headerCrc32', False),
        ('version2', [u32(0x04, 2)], 'version 2', True),
        ('magic_lodo', [('4s', 0, b'LODO')], 'LODO', True),
        ('reserved_hdr_0x98', [('B', 0x98, 1)], 'reserved header byte at 0x98', True),
        ('reserved_hdr_0x3f', [('B', 0x3F, 1)], 'reserved header byte at 0x3f', True),
        ('azimuths_8', [('<H', 0x3C, 8)], 'azimuth count', True),
        ('header_flag_unknown', [u32(0x08, 3)], 'unknown bits', True),
        ('grid_0', [('B', 0x3E, 0)], 'terrain grid 0', True),
        ('options_hash', [('<Q', 0x28, S['optionsHash'] ^ 1)], 'optionsHash', True),
        ('patch_table_offset', [('<Q', 0x80, L['patch'] + 64)], 'patch table at', True),
        ('cell_reserved', [('<H', L['cell'] + 32 * ci + 30, 1)], 'cell %d: reserved' % ci, True),
        ('cell_quest_layer', [('<H', L['cell'] + 32 * ci + 28, 3)], 'quest-layer flag is PROPOSED', True),
        ('cell_first_patch', [u32(L['cell'] + 32 * ci, S['cells'][ci]['firstPatch'] + 1)], 'patches are in cell order', True),
        ('instance_unsorted', None, 'not sorted by lodiIndex', True),
        ('chart_mesh_id', [('B', L['chart'] + 64 * ch0 + 10, 7)], 'is not its place', True),
        ('chart_reserved', [u32(L['chart'] + 64 * ch0 + 60, 1)], 'chart 0: reserved', True),
        ('chart_step_neg', [('<f', L['chart'] + 64 * ch0 + 12, -1.0)], 'step', True),
        ('chart_axes', [('<f', L['chart'] + 64 * ch0 + 28, 2.0)], 'not orthonormal', True),
        ('chart_instance', [u32(L['chart'] + 64 * ch0 + 52, 99)], 'names instance 99', True),
        ('slot_past', [u32(L['slot'] + 4 * S['charts'][ch_obj]['firstSlot'] + 4 * next(
            k for k in range(S['charts'][ch_obj]['nu'] * S['charts'][ch_obj]['nv'] * S['charts'][ch_obj]['nw'])
            if S['slots'][S['charts'][ch_obj]['firstSlot'] + k] != NONE), len(S['patches']) + 5)],
         'is past', True),
        ('fallback_foreign', [u32(L['chart'] + 64 * ch_obj + 56, terr)], 'fallback patch', True),
        ('patch_kind_water', [('B', L['patch'] + 64 * obj + 15, K_WATER)], 'kind water is reserved', True),
        ('patch_reserved', [u32(L['patch'] + 64 * obj + 60, 1)], 'patch %d: reserved' % obj, True),
        ('patch_area_zero', [('<H', L['patch'] + 64 * obj + 12, 0)], 'area', True),
        ('patch_submerged_object', [('B', L['patch'] + 64 * obj + 11, SUBMERGED)], 'only terrain is flagged submerged', True),
        ('terrain_lodsource', [u32(L['patch'] + 64 * terr + 56, 63 if S['patches'][terr]['lodSource'] != 63 else 0)],
         'terrain lodSource', True),
        ('patch_flags_unknown', [('B', L['patch'] + 64 * obj + 11, 0x80)], 'unknown bits', True),
    ]
    manifest = []
    bad = 0
    for name, edits, want, rs in cases:
        m = bytearray(b)
        if name == 'instance_unsorted':
            if len(S['instances']) < 2:
                print('control %s: SKIPPED (one instance)' % name)
                continue
            struct.pack_into('<I', m, L['inst'] + 16, S['instances'][0]['lodiIndex'])
        else:
            for fmt, off, v in edits:
                if v is None:
                    m[off] ^= 0x01
                else:
                    struct.pack_into(fmt, m, off, v)
        if rs:
            m = bytearray(resign(bytes(m)))
        p = os.path.join(outdir, name + '.lodp')
        open(p, 'wb').write(m)
        try:
            read_lodp(bytes(m))
            verdict = 'ACCEPTED'
        except Refusal as e:
            verdict = 'REFUSED: %s' % e
        ok = verdict.startswith('REFUSED') and want in verdict
        bad += not ok
        print('control %-24s %s  [%s] %s' % (name, 'ok ' if ok else 'BAD', want, verdict))
        manifest.append('%s\t%s' % (p.replace('\\', '/'), want))
    # the unmutated, re-signed control must be accepted
    p = os.path.join(outdir, 'resigned_good.lodp')
    open(p, 'wb').write(resign(b))
    try:
        read_lodp(resign(b))
        print('control resigned_good             ok  ACCEPTED')
    except Refusal as e:
        bad += 1
        print('control resigned_good             BAD REFUSED: %s' % e)
    manifest.append('%s\t%s' % (p.replace('\\', '/'), 'ACCEPTED'))
    open(os.path.join(outdir, 'controls.tsv'), 'w').write('\n'.join(manifest) + '\n')
    print('%d controls, %d bad (python reader)' % (len(manifest), bad))
    return 1 if bad else 0


def lodo_controls(lodo, outdir):
    """Tier-2 controls on the `.lodo` v8 chart stream: each mutant re-signed (indexCrc32 over every table,
    the stream included, then headerCrc32) so the stream RULE answers, in both readers."""
    import lodgen_native_mutate as mu
    b = open(lodo, 'rb').read()
    L = nd.read_lodo(lodo)
    h = L['header']
    off = h['offChartStream']
    os.makedirs(outdir, exist_ok=True)
    cl = L['clusters']
    short = next(i for i, c in enumerate(cl) if c['triangleCount'] < 16)
    m0, m1 = L['meshes'][0], L['meshes'][1]
    coarse = next(c for c in range(m1['clusterFirst'], m1['clusterFirst'] + m1['clusterCount'])
                  if L['clusterLods'][c]['level'] > 0)
    cases = []

    def stream_edit(name, edits, want):
        m = bytearray(b)
        for at, v in edits:
            m[off + at] = v
        cases.append((name, m, want, True))
    stream_edit('lodo_past_triangles', [(short * 16 + cl[short]['triangleCount'], 0)], 'past its')
    stream_edit('lodo_fd', [(m0['clusterFirst'] * 16, 0xFD)], '0xfd')
    ones = [(c * 16 + t, 0) for c in range(m0['clusterFirst'], m0['clusterFirst'] + m0['clusterCount'])
            for t in range(cl[c]['triangleCount']) if L['chartStream'][c * 16 + t] == 1]
    stream_edit('lodo_not_dense', ones, 'not dense')
    stream_edit('lodo_coarse_past', [(coarse * 16, 3)], 'coarser')
    m = bytearray(b)
    struct.pack_into('<Q', m, 0xE0, 0)
    cases.append(('lodo_v8_offset0', m, 'no chart stream', True))
    m = bytearray(b)
    struct.pack_into('<I', m, 0x04, 7)
    cases.append(('lodo_v7_no_near', m, 'NEAR flag', True))
    m = bytearray(b)
    struct.pack_into('<I', m, 0x04, 6)
    cases.append(('lodo_v6_with_offset', m, 'reserved', True))
    m = bytearray(b)
    m[off + 3] ^= 0x01
    cases.append(('lodo_t1_flip_stream', m, 'indexCrc32', False))
    manifest, bad = [], 0
    for name, m, want, rs in cases:
        if rs:
            mu.resign_lodo(m)
        p = os.path.join(outdir, name + '.lodo')
        open(p, 'wb').write(m)
        try:
            nd.read_lodo(p)
            v = 'ACCEPTED'
        except nd.Refusal as e:
            v = 'REFUSED: %s' % e
        ok = v.startswith('REFUSED') and want in v
        bad += not ok
        print('control %-24s %s  [%s] %s' % (name, 'ok ' if ok else 'BAD', want, v))
        manifest.append('%s\t%s' % (p.replace('\\', '/'), want))
    m = bytearray(b)
    mu.resign_lodo(m)
    p = os.path.join(outdir, 'lodo_resigned_good.lodo')
    open(p, 'wb').write(m)
    try:
        nd.read_lodo(p)
        print('control lodo_resigned_good        ok  ACCEPTED')
    except nd.Refusal as e:
        bad += 1
        print('control lodo_resigned_good        BAD REFUSED: %s' % e)
    manifest.append('%s\tACCEPTED' % p.replace('\\', '/'))
    open(os.path.join(outdir, 'lodo_controls.tsv'), 'w').write('\n'.join(manifest) + '\n')
    print('%d controls, %d bad (python reader)' % (len(manifest), bad))
    return 1 if bad else 0


def judge(manifest, output):
    """The other reader's lines: 'REFUSED <path>: <why>' or 'ACCEPTED <path>'."""
    verdicts = {}
    for line in open(output, encoding='utf-8', errors='replace'):
        line = line.strip()
        if line.startswith('REFUSED '):
            path, _, why = line[8:].partition(': ')
            verdicts[os.path.basename(path)] = 'REFUSED: ' + why
        elif line.startswith('ACCEPTED '):
            verdicts[os.path.basename(line[9:])] = 'ACCEPTED'
    bad = 0
    n = 0
    for line in open(manifest):
        path, want = line.rstrip('\n').split('\t')
        v = verdicts.get(os.path.basename(path), 'MISSING')
        n += 1
        ok = (v == 'ACCEPTED') if want == 'ACCEPTED' else (v.startswith('REFUSED') and want in v)
        bad += not ok
        print('c++ %-28s %s  [%s] %s' % (os.path.basename(path), 'ok ' if ok else 'BAD', want, v))
    print('%d controls, %d bad (C++ reader)' % (n, bad))
    return 1 if bad else 0


def main(argv):
    if len(argv) >= 2 and argv[0] == '--fixture':
        red = argv[argv.index('--expect-red') + 1] if '--expect-red' in argv else None
        return run_fixture(argv[1], red)
    if len(argv) >= 4 and argv[0] == '--real':
        cap = int(argv[argv.index('--sample') + 1]) if '--sample' in argv else 400
        reg = None
        if '--region' in argv:
            k = argv.index('--region')
            reg = tuple(int(v) for v in argv[k + 1:k + 5])
        return real_check(argv[1], argv[2], argv[3], cap, reg)
    if len(argv) == 3 and argv[0] == '--controls':
        return controls(argv[1], argv[2])
    if len(argv) == 3 and argv[0] == '--lodo-controls':
        return lodo_controls(argv[1], argv[2])
    if len(argv) == 3 and argv[0] == '--judge':
        return judge(argv[1], argv[2])
    if len(argv) == 2 and argv[0] == '--stream':
        L = nd.read_lodo(argv[1])
        if L['chartStream'] is None:
            print('REFUSED %s: no chart stream (version %d)' % (argv[1], L['header']['version']))
            return 1
        stream, per = chart_stream(L)
        diff = sum(1 for a, b in zip(stream, L['chartStream']) if a != b)
        nch = sum(p['charts'] for p in per)
        print('lodp-stream %s: %d meshes, %d charts, %d tree meshes, %d of %d bytes differ'
              % (argv[1], len(per), nch, sum(1 for p in per if p['foliage']), diff, len(stream)))
        print('PASS' if diff == 0 else 'FAIL')
        return 0 if diff == 0 else 1
    if argv and not argv[0].startswith('--'):
        rc = 0
        for path in argv:
            try:
                S = read_lodp(open(path, 'rb').read())
                kinds = [sum(1 for p in S['patches'] if p['kind'] == k) for k in range(4)]
                print('ACCEPTED %s: sector %d %d, %d instances, %d charts, %d slots, %d patches '
                      '(terrain %d, object %d, tree %d), %d bytes'
                      % (path, S['sectorX'], S['sectorY'], len(S['instances']), len(S['charts']), len(S['slots']),
                         len(S['patches']), kinds[0], kinds[1], kinds[3], S['fileBytes']))
            except Refusal as e:
                print('REFUSED %s: %s' % (path, e))
                rc = 1
        return rc
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
