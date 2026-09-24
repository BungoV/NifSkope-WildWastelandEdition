#!/usr/bin/env python3
"""IDENT lane -- part 2.

(1) SCOL taken by BASE TYPE, because the architecture MODL rule scores 0% on
    them: a SCOL's own MODL is an auto-generated 'SCOL\\...' path, so the rule
    as written cannot see one.  Reported separately and said plainly.
(2) Is a LAYR one building?  Measured, not asserted: inside one layer, run
    spatial connected components over the REFRs' rotated world bounds and count
    how many disjoint blobs a layer holds.
(3) The layer TREE (PNAM parents): depth, and whether leaves are per-building.
"""
import math
import os
import pickle
import struct
import sys
from collections import Counter, defaultdict

LANE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = ('C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/'
           '392777f8-9016-4913-858d-16d6eec4c01a/scratchpad')
sys.path.insert(0, LANE)
from ident_measure import is_arch, cell, inbox, diag, med, PRIMARY, WIDE  # noqa


def world_aabb(ref, bases):
    """Rotated-then-axis-aligned world box of a REFR from its base OBND."""
    form, base, x, y, z, rx, ry, rz, sc = ref
    b = bases.get(base)
    if not b or not b.get('obnd'):
        return None
    o = b['obnd']
    cx, cy, cz, sx, sy, sz = (math.cos(rx), math.cos(ry), math.cos(rz),
                              math.sin(rx), math.sin(ry), math.sin(rz))
    # Bethesda REFR rotation: X then Y then Z, applied as Rz*Ry*Rx
    m = [[cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz],
         [cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz],
         [-sy,     sx * cy,                cx * cy]]
    lo = [1e30] * 3
    hi = [-1e30] * 3
    for i in (0, 3):
        for j in (1, 4):
            for k in (2, 5):
                p = (o[i] * sc, o[j] * sc, o[k] * sc)
                for a in range(3):
                    v = m[a][0] * p[0] + m[a][1] * p[1] + m[a][2] * p[2]
                    lo[a] = min(lo[a], v)
                    hi[a] = max(hi[a], v)
    return (x + lo[0], y + lo[1], z + lo[2], x + hi[0], y + hi[1], z + hi[2])


def components(boxes, pad=0.0):
    """Connected components of overlapping (padded) AABBs.  Returns labels."""
    n = len(boxes)
    parent = list(range(n))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb

    # bucket by 512-unit grid cell to keep the pair test local
    grid = defaultdict(list)
    for i, bx in enumerate(boxes):
        if bx is None:
            continue
        for gx in range(int((bx[0] - pad) // 512), int((bx[3] + pad) // 512) + 1):
            for gy in range(int((bx[1] - pad) // 512), int((bx[4] + pad) // 512) + 1):
                grid[(gx, gy)].append(i)
    for cellids in grid.values():
        for a in range(len(cellids)):
            ia = cellids[a]
            ba = boxes[ia]
            for b in range(a + 1, len(cellids)):
                ib = cellids[b]
                bb = boxes[ib]
                if (ba[0] - pad <= bb[3] and bb[0] - pad <= ba[3]
                        and ba[1] - pad <= bb[4] and bb[1] - pad <= ba[4]
                        and ba[2] - pad <= bb[5] and bb[2] - pad <= ba[5]):
                    union(ia, ib)
    return [find(i) if boxes[i] is not None else -1 for i in range(n)]


def main():
    E = pickle.load(open(os.path.join(LANE, 'esm.pkl'), 'rb'))
    S = pickle.load(open(os.path.join(SCRATCH, 'ident_scan.pkl'), 'rb'))
    bases, refs = E['bases'], E['refs']
    refsub, layr, scol = S['refsub'], S['layr'], S['scol']
    out = []

    def say(s=''):
        out.append(s)
        print(s)

    # ------------------------------------------------- (1) SCOL by base type
    for name, box in (('PRIMARY', PRIMARY), ('WIDE', WIDE)):
        allr = [r for r in refs if inbox(r[2], r[3], box)]
        sc = [r for r in allr if bases.get(r[1], {}).get('type') == 'SCOL']
        say('%s: %d REFRs, %d of them SCOL instances = %.2f%% of ALL REFRs'
            % (name, len(allr), len(sc), 100.0 * len(sc) / max(1, len(allr))))
        if not sc:
            continue
        pc, dg, archparts = [], [], 0
        for r in sc:
            s = scol.get(r[1])
            if not s:
                continue
            n = sum(max(1, nb // 28) for _, nb in s['parts'])
            pc.append(n)
            o = s['obnd']
            if o:
                dg.append(math.sqrt((o[3] - o[0]) ** 2 + (o[4] - o[1]) ** 2
                                    + (o[5] - o[2]) ** 2))
            if any(is_arch(bases.get(p, {}).get('modl', '')) for p, _ in s['parts']):
                archparts += 1
        say('  median part placements %s (mean %.1f), median OBND diagonal %.0f '
            'units = %.1f m' % (med(pc), sum(pc) / float(len(pc)), med(dg),
                                med(dg) * 0.0142875))
        say('  SCOL instances whose PARTS include an architecture mesh: %d of %d'
            % (archparts, len(sc)))
        say('  five most-placed SCOL bases:')
        for f, n in Counter(r[1] for r in sc).most_common(5):
            s = scol.get(f, {})
            o = s.get('obnd')
            d = (math.sqrt((o[3] - o[0]) ** 2 + (o[4] - o[1]) ** 2
                           + (o[5] - o[2]) ** 2) if o else 0)
            np_ = sum(max(1, nb // 28) for _, nb in s.get('parts', []))
            ap = sum(1 for p, _ in s.get('parts', [])
                     if is_arch(bases.get(p, {}).get('modl', '')))
            say('    %08X %-40s parts=%3d (arch %d) obndDiag=%6.0f (%.1f m) x%d'
                % (f, s.get('edid', '?'), np_, ap, d, d * 0.0142875, n))
        say()

    # ------------------------------------- (2) is a layer one building?
    for name, box in (('PRIMARY', PRIMARY), ('WIDE', WIDE)):
        allr = [r for r in refs if inbox(r[2], r[3], box)]
        arch = [r for r in allr if is_arch(bases.get(r[1], {}).get('modl', ''))]
        byLayer = defaultdict(list)
        for r in arch:
            s = refsub.get(r[0], {})
            if b'XLYR' in s:
                byLayer[struct.unpack_from('<I', s[b'XLYR'][0], 0)[0]].append(r)
        say('--- %s: layers as buildings (%d layers) ---' % (name, len(byLayer)))
        comps = []
        rows = []
        for f, v in byLayer.items():
            boxes = [world_aabb(r, bases) for r in v]
            if all(b is None for b in boxes):
                continue
            lab = components(boxes, pad=0.0)
            nc = len(set(l for l in lab if l >= 0))
            # biggest component's share
            cc = Counter(l for l in lab if l >= 0)
            big = cc.most_common(1)[0][1] if cc else 0
            comps.append(nc)
            rows.append((len(v), nc, big, diag([(r[2], r[3], r[4]) for r in v]),
                         layr.get(f, ('?', 0))[0], f))
        say('  blobs per layer: median %s, mean %.1f, histogram %s'
            % (med(comps), sum(comps) / float(max(1, len(comps))),
               dict(sorted(Counter(comps).items())[:12])))
        say('  layers holding exactly ONE touching blob: %d of %d = %.1f%%'
            % (sum(1 for c in comps if c == 1), len(comps),
               100.0 * sum(1 for c in comps if c == 1) / max(1, len(comps))))
        rows.sort(key=lambda t: -t[0])
        say('  %-44s %6s %6s %6s %9s' % ('layer', 'refrs', 'blobs', 'big', 'diag'))
        for n, nc, big, d, ed, f in rows[:20]:
            say('  %-44s %6d %6d %6d %9.0f' % (ed[:44], n, nc, big, d))
        say()

    # ------------------------------------------- (3) the layer tree
    kids = defaultdict(list)
    for f, (ed, par) in layr.items():
        kids[par].append(f)
    depth = {}

    def d_of(f, seen=None):
        if f in depth:
            return depth[f]
        seen = seen or set()
        if f in seen:
            return 0
        seen.add(f)
        par = layr.get(f, ('', 0))[1]
        depth[f] = 0 if not par else 1 + d_of(par, seen)
        return depth[f]

    for f in layr:
        d_of(f)
    say('LAYR tree: %d layers, depth histogram %s, roots %d, leaves %d'
        % (len(layr), dict(sorted(Counter(depth.values()).items())),
           sum(1 for f in layr if not layr[f][1]),
           sum(1 for f in layr if f not in kids)))
    with open(os.path.join(SCRATCH, 'ident_group.txt'), 'w') as fh:
        fh.write('\n'.join(out))


if __name__ == '__main__':
    main()
