#!/usr/bin/env python3
"""LODLEVELS1 -- does the whole-building LOD object STAND WHERE the kit pieces
stand, or is it a different structure nearby?

verify.py counted refs inside a widened SQUARE around each whole-building LOD.
A square that big can swallow a neighbour, so the claim "the kit pieces are
still drawn under it at LOD8" is checked here against the mesh's own world AABB
in all THREE axes, with no widening at all.
"""
import os
import pickle
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT + '/tests/spells')
import gltf_nifread as NR                   # noqa: E402
import lodgen_native_decode as D            # noqa: E402

DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
CELL = 4096.0
_c = {}


def bbox(rel):
    if not rel:
        return None
    k = rel.lower()
    if k in _c:
        return _c[k]
    p = os.path.join(DATA, 'Meshes', rel.replace('\\', '/'))
    out = None
    if os.path.exists(p):
        try:
            n = NR.Nif(p)
            lo = [1e30] * 3
            hi = [-1e30] * 3
            tris = 0
            for sh in n.shapes.values():
                tris += sh['numTris']
                chain = []
                cur = sh
                g = 0
                while cur is not None and g < 64:
                    chain.append((cur['t'], cur['r'], cur['s']))
                    pa = cur.get('parent')
                    cur = n.nodes.get(pa) if pa is not None else None
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
            if lo[0] < 1e29:
                out = (tris, tuple(lo), tuple(hi))
        except Exception:                            # noqa: BLE001
            out = None
    _c[k] = out
    return out


def isneigh(p):
    return 'neighborhoods' in (p or '').lower().replace('/', '\\').split('\\')


def main():
    d = pickle.load(open(os.path.join(HERE, 'esm.pkl'), 'rb'))
    bases, refs = d['bases'], d['refs']
    whole = {f: [k for k in range(4) if isneigh(v['slots'][k])]
             for f, v in bases.items() if any(isneigh(s) for s in v['slots'])}
    DX0, DY0, DX1, DY1 = 0 * CELL, -16 * CELL, 12 * CELL, -4 * CELL
    inbox = [r for r in refs if DX0 <= r[2] < DX1 and DY0 <= r[3] < DY1]
    wrefs = [r for r in inbox if r[1] in whole]
    print('whole-building LOD placements in the downtown box: %d' % len(wrefs))

    # per-slot totals over EXACT 3D AABBs
    tot = {1: [0, 0, 0], 2: [0, 0, 0]}
    per = []
    for r in wrefs:
        f = r[1]
        k = min(whole[f])
        if k not in tot:
            continue
        bb = bbox(bases[f]['slots'][k])
        if not bb:
            continue
        tris, lo, hi = bb
        # the placement's rotation about Z is applied to the box by taking the
        # larger of the two horizontal extents on both axes -- an over-estimate
        # in x/y, exact in z, so a ref counted INSIDE is generous to the claim
        # and a ref counted OUTSIDE is beyond argument
        s = r[8]
        hx = max(hi[0] - lo[0], hi[1] - lo[1]) * 0.5 * s
        cx = r[2] + (lo[0] + hi[0]) * 0.5 * s
        cy = r[3] + (lo[1] + hi[1]) * 0.5 * s
        z0, z1 = r[4] + lo[2] * s, r[4] + hi[2] * s
        ins = [q for q in inbox
               if q[0] != r[0] and abs(q[2] - cx) <= hx and abs(q[3] - cy) <= hx
               and z0 - 128.0 <= q[4] <= z1 + 128.0]
        still = [q for q in ins if bases.get(q[1]) and bases[q[1]]['slots'][k]]
        kit = [q for q in still
               if not isneigh(bases[q[1]]['slots'][k])]
        tot[k][0] += len(ins)
        tot[k][1] += len(still)
        tot[k][2] += len(kit)
        per.append((len(ins), len(still), len(kit), k, f, tris, hx, z1 - z0))

    for k in (1, 2):
        n = sum(1 for p in per if p[3] == k)
        a, b, c = tot[k]
        print('\nwhole-building objects whose FIRST slot is %d (LOD%d): %d placements'
              % (k, (4, 8, 16, 32)[k], n))
        print('  refs standing inside their exact 3D box: %d' % a)
        print('  of those, STILL drawn at that same level: %d (%.1f%%)'
              % (b, 100.0 * b / max(a, 1)))
        print('  of those still drawn, NOT themselves a whole-building object: %d' % c)

    per.sort(reverse=True)
    print('\ntop ten by refs inside the exact box:')
    print('  inside | still | kit | slot | tris | halfXY |   dZ  | editorID')
    for a, b, c, k, f, tris, hx, dz in per[:10]:
        print('  %6d | %5d | %3d |  %d   | %4d | %6.0f | %5.0f | %s'
              % (a, b, c, k, tris, hx, dz, bases[f]['edid'][:44]))

    # ---- our .lodi v7 groups on the same chunk, for section 4
    print('\n' + '=' * 70)
    print('OUR OWN GROUPING on chunk 4.4.-12, for the shadow-identity question')
    try:
        T = D.read_lodi(ROOT + '/scratchpad/horizon2_20260918/dumpbake/nat/'
                        'FO4CSLOD/Commonwealth/Commonwealth.lodi')
        h = T['header']
        print('  .lodi v%d, instances %d' % (h.get('version'), h.get('instanceCount', -1)))
        for key in sorted(h):
            if 'group' in key.lower() or 'Group' in key:
                print('    %s = %s' % (key, h[key]))
        for key in sorted(T):
            if 'group' in key.lower():
                v = T[key]
                print('    table %s: %d rows' % (key, len(v) if hasattr(v, '__len__') else -1))
    except Exception as e:                           # noqa: BLE001
        print('  .lodi read refused: %s' % e)


if __name__ == '__main__':
    main()
