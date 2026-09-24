#!/usr/bin/env python3
"""LODLEVELS1 -- the two verifications the answer stands on.

A. THE SLOT -> LEVEL CONTROL.  Bethesda's own baked object chunks
   (Meshes/Terrain/Commonwealth/Objects/Commonwealth.<L>.<x>.<y>.BTO) are the
   ground truth for what the engine draws at level L.  For each level we predict
   the triangle count of one chunk from the ESM alone -- every placement in the
   chunk's cells whose base fills MNAM slot k -- and compare it with the .BTO's
   own triangle count.  If slot k really is level L, the two agree; if they do
   not, the mapping in the report is wrong and the report says so.

B. DO THE KIT PIECES DROP OUT UNDER A WHOLE-BUILDING LOD?  For every
   whole-building LOD placement in downtown, the refs standing inside its
   footprint are counted and split by whether they still have a mesh at that
   level.
"""
import collections
import math
import os
import pickle
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT + '/tests/spells')
import gltf_nifread as NR                   # noqa: E402

DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
BTODIR = DATA + '/Meshes/Terrain/Commonwealth/Objects'
CELL = 4096.0
_cache = {}


def local_bbox_and_tris(rel):
    """(tris, verts list transformed to model space, bbox) for a mesh path."""
    if not rel:
        return None
    key = rel.lower().replace('\\', '/')
    if key in _cache:
        return _cache[key]
    path = os.path.join(DATA, 'Meshes', rel.replace('\\', '/'))
    out = None
    if os.path.exists(path):
        try:
            n = NR.Nif(path)
            tris = 0
            lo = [1e30] * 3
            hi = [-1e30] * 3
            any_v = False
            for sh in n.shapes.values():
                tris += sh['numTris']
                chain = []
                cur = sh
                g = 0
                while cur is not None and g < 64:
                    chain.append((cur['t'], cur['r'], cur['s']))
                    p = cur.get('parent')
                    cur = n.nodes.get(p) if p is not None else None
                    g += 1
                for v in sh['verts']:
                    any_v = True
                    x, y, z = v
                    for (t, r, s) in chain:
                        x, y, z = (r[0] * x + r[1] * y + r[2] * z,
                                   r[3] * x + r[4] * y + r[5] * z,
                                   r[6] * x + r[7] * y + r[8] * z)
                        x, y, z = x * s + t[0], y * s + t[1], z * s + t[2]
                    for i, c in enumerate((x, y, z)):
                        lo[i] = min(lo[i], c)
                        hi[i] = max(hi[i], c)
            out = (tris, (tuple(lo), tuple(hi)) if any_v else None)
        except Exception:                            # noqa: BLE001
            out = None
    _cache[key] = out
    return out


def bto_tris(path):
    """(triangles, shapes, world bbox) of a baked object chunk."""
    n = NR.Nif(path)
    tris = 0
    lo = [1e30] * 3
    hi = [-1e30] * 3
    for sh in n.shapes.values():
        tris += sh['numTris']
        chain = []
        cur = sh
        g = 0
        while cur is not None and g < 64:
            chain.append((cur['t'], cur['r'], cur['s']))
            p = cur.get('parent')
            cur = n.nodes.get(p) if p is not None else None
            g += 1
        for v in sh['verts']:
            x, y, z = v
            for (t, r, s) in chain:
                x, y, z = (r[0] * x + r[1] * y + r[2] * z,
                           r[3] * x + r[4] * y + r[5] * z,
                           r[6] * x + r[7] * y + r[8] * z)
                x, y, z = x * s + t[0], y * s + t[1], z * s + t[2]
            for i, c in enumerate((x, y, z)):
                lo[i] = min(lo[i], c)
                hi[i] = max(hi[i], c)
    return tris, len(n.shapes), (tuple(lo), tuple(hi))


def main():
    d = pickle.load(open(os.path.join(HERE, 'esm.pkl'), 'rb'))
    bases, refs = d['bases'], d['refs']

    print('=' * 78)
    print('A. SLOT -> LEVEL CONTROL against Bethesda\'s own .BTO bakes')
    print('   window held in esm.pkl is cells x -4..19, y -24..3, so only chunks')
    print('   inside it can be predicted; the rest are marked partial.')
    print('-' * 78)
    print('level | chunk    | cells                  | .BTO tris | shapes | '
          'ESM slot | predicted tris | placements')
    for lvl, cx, cy in ((4, 4, -12), (4, 4, -16), (8, 0, -16), (8, 8, -16),
                        (16, 0, -16), (16, 16, -16), (32, 0, -32)):
        p = os.path.join(BTODIR, 'Commonwealth.%d.%d.%d.BTO' % (lvl, cx, cy))
        if not os.path.exists(p):
            print(' %2d   | %4d.%-4d | MISSING %s' % (lvl, cx, cy, os.path.basename(p)))
            continue
        bt, nsh, bb = bto_tris(p)
        k = {4: 0, 8: 1, 16: 2, 32: 3}[lvl]
        x0, y0 = cx * CELL, cy * CELL
        x1, y1 = (cx + lvl) * CELL, (cy + lvl) * CELL
        wx0, wy0, wx1, wy1 = (v * CELL for v in d['window'])
        partial = not (wx0 <= x0 and wy0 <= y0 and x1 <= wx1 and y1 <= wy1)
        pt = 0
        np_ = 0
        for r in refs:
            if not (x0 <= r[2] < x1 and y0 <= r[3] < y1):
                continue
            b = bases.get(r[1])
            if not b or not b['slots'][k]:
                continue
            m = local_bbox_and_tris(b['slots'][k])
            if not m:
                continue
            pt += m[0]
            np_ += 1
        print(' %2d   | %4d.%-4d | x%d..%d y%d..%d%s | %9d | %6d | slot %d   | %14d | %10d'
              % (lvl, cx, cy, cx, cx + lvl - 1, cy, cy + lvl - 1,
                 ' PARTIAL' if partial else '        ', bt, nsh, k, pt, np_))

    # ---------------------------------------------------------------- part B
    print('\n' + '=' * 78)
    print('B. DO THE KIT PIECES DROP OUT UNDER A WHOLE-BUILDING LOD?')
    print('-' * 78)
    # every placed base whose LOD mesh lives under LOD\Neighborhoods -- that is
    # Bethesda\'s own folder for the per-building LOD objects
    whole = {}
    for f, v in bases.items():
        for k in range(4):
            s = v['slots'][k]
            if s and 'neighborhoods' in s.lower().replace('/', '\\').split('\\'):
                whole.setdefault(f, []).append(k)
    print('bases whose MNAM mesh is under LOD\\Neighborhoods: %d' % len(whole))
    pat = collections.Counter(tuple(v) for v in whole.values())
    for k, n in pat.most_common():
        print('   slots %-12s %5d' % (str(k), n))

    # downtown placements of them
    DX0, DY0, DX1, DY1 = 0 * CELL, -16 * CELL, 12 * CELL, -4 * CELL
    wrefs = [r for r in refs if r[1] in whole and DX0 <= r[2] < DX1 and DY0 <= r[3] < DY1]
    print('\nwhole-building LOD placements inside the downtown box '
          '(cells x0..11, y-16..-5): %d' % len(wrefs))
    allrefs = [r for r in refs if DX0 <= r[2] < DX1 and DY0 <= r[3] < DY1]
    print('all placements in that box: %d' % len(allrefs))

    rows = []
    for r in wrefs:
        f = r[1]
        k = min(whole[f])
        m = local_bbox_and_tris(bases[f]['slots'][k])
        if not m or not m[1]:
            continue
        lo, hi = m[1]
        # axis-aligned world box (rotations of these are pure Z or none; the box
        # is widened by the larger horizontal half-extent so a yaw cannot shrink it)
        hx = max(hi[0] - lo[0], hi[1] - lo[1]) * 0.5 * r[8]
        cxw = r[2] + (lo[0] + hi[0]) * 0.5 * r[8]
        cyw = r[3] + (lo[1] + hi[1]) * 0.5 * r[8]
        inside = [q for q in allrefs
                  if abs(q[2] - cxw) <= hx and abs(q[3] - cyw) <= hx and q[0] != r[0]]
        still = [q for q in inside
                 if bases.get(q[1]) and bases[q[1]]['slots'][k]]
        rows.append((len(inside), len(still), r, f, k, hx, m[0]))
    rows.sort(reverse=True)
    tin = sum(x[0] for x in rows)
    tst = sum(x[1] for x in rows)
    print('\nover %d whole-building LOD placements: %d refs stand inside their '
          'footprints, of which %d (%.1f%%) STILL have a mesh at that same level'
          % (len(rows), tin, tst, 100.0 * tst / max(tin, 1)))
    print('\nthe twelve with the most refs under them:')
    print('  refs inside | still drawn | tris | half-extent | editorID')
    for nin, nst, r, f, k, hx, tr in rows[:12]:
        print('  %11d | %11d | %4d | %11.0f | %s (slot %d, %08X)'
              % (nin, nst, tr, hx, bases[f]['edid'][:40], k, f))
        if nst:
            names = collections.Counter()
            for q in allrefs:
                if abs(q[2] - (r[2])) <= hx * 2 and bases.get(q[1]) and bases[q[1]]['slots'][k]:
                    names[bases[q[1]]['edid'][:38]] += 1
            print('      still-drawn neighbours: %s'
                  % ', '.join('%s x%d' % (a, b) for a, b in names.most_common(4)))


if __name__ == '__main__':
    main()
