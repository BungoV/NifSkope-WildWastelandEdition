#!/usr/bin/env python3
"""LODLEVELS1 sections 1-3 -- at which vanilla object LOD level does a downtown
building stop being forty kit pieces and become one mesh?

Read-only.  Everything comes from Fallout4.esm (via esm.pkl) and the unpacked
vanilla meshes.  Nothing is written outside this lane's folder.
"""
import collections
import os
import pickle
import statistics
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT + '/tests/spells')
import gltf_nifread as NR                   # noqa: E402

DATA = 'E:/Tools/Fallout 4/DataUnpacked/Data'
CELL = 4096.0
# chunk 4.4.-12 == cells x 4..7, y -12..-9 -- the urban region every LOD lane uses
CHUNK = (4 * CELL, -12 * CELL, 8 * CELL, -8 * CELL)
# a wider downtown box for the "share of downtown buildings" question
DOWNTOWN = (0 * CELL, -16 * CELL, 12 * CELL, -4 * CELL)

_cache = {}


def measure(rel):
    """(tris, verts, (dx,dy,dz), diag) for a model path relative to Meshes\\."""
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
            tris = verts = 0
            lo = [1e30] * 3
            hi = [-1e30] * 3
            for sh in n.shapes.values():
                tris += sh['numTris']
                verts += sh['numVerts']
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
            if verts:
                e = tuple(hi[i] - lo[i] for i in range(3))
                out = (tris, verts, e, (e[0] ** 2 + e[1] ** 2 + e[2] ** 2) ** 0.5)
            else:
                out = (tris, 0, (0.0, 0.0, 0.0), 0.0)
        except Exception:                            # noqa: BLE001
            out = None
    _cache[key] = out
    return out


def kitrule(modl, slots):
    """bungo's question is about KIT PIECES: a wall/roof/window piece of a
    building kit, each its own REFR.  The rule, stated so it can be argued with:
    the NEAR model path (or, absent one, the slot path) has a component that is
    `architecture`, `buildings`, or ends in `kit` (MetalKit, DecoKit, BrickKit,
    WoodKit, ...)."""
    p = (modl or slots[0] or '').lower().replace('/', '\\')
    parts = p.split('\\')[:-1]
    for c in parts:
        if c in ('architecture', 'buildings') or c.endswith('kit'):
            return True
    return False


def obnd_diag(o):
    if not o:
        return 0.0
    dx, dy, dz = o[3] - o[0], o[4] - o[1], o[5] - o[2]
    return (dx * dx + dy * dy + dz * dz) ** 0.5


def med(v):
    return statistics.median(v) if v else float('nan')


def region_refs(refs, box):
    x0, y0, x1, y1 = box
    return [r for r in refs if x0 <= r[2] < x1 and y0 <= r[3] < y1]


def main():
    d = pickle.load(open(os.path.join(HERE, 'esm.pkl'), 'rb'))
    bases, cw, refs = d['bases'], d['cwcount'], d['refs']
    print('=' * 78)
    print('INPUTS: Fallout4.esm %d bases, %d Commonwealth placements; window %s'
          % (len(bases), sum(cw.values()), d['window']))

    for name, box in (('chunk 4.4.-12 (cells x4..7, y-12..-9)', CHUNK),
                      ('downtown box (cells x0..11, y-16..-5)', DOWNTOWN)):
        rr = region_refs(refs, box)
        bs = collections.Counter(r[1] for r in rr)
        print('\n' + '=' * 78)
        print('REGION %s: %d placements, %d distinct bases' % (name, len(rr), len(bs)))
        print('-' * 78)
        print('slot | level | bases w/ mesh |   kit |  other | placements |  kit plc | '
              'med mesh diag | med kit diag | med other diag')
        for k, lvl in enumerate((4, 8, 16, 32)):
            hb = [b for b in bs if b in bases and bases[b]['slots'][k]]
            kit = [b for b in hb if kitrule(bases[b]['modl'], bases[b]['slots'])]
            oth = [b for b in hb if b not in set(kit)]
            plc = sum(bs[b] for b in hb)
            kplc = sum(bs[b] for b in kit)
            dg, dk, do = [], [], []
            for b in hb:
                m = measure(bases[b]['slots'][k])
                if not m:
                    continue
                dg.append(m[3])
                (dk if kitrule(bases[b]['modl'], bases[b]['slots']) else do).append(m[3])
            print(' %d   |  %2d   | %13d | %5d | %6d | %10d | %8d | %13.0f | %12.0f | %14.0f'
                  % (k, lvl, len(hb), len(kit), len(oth), plc, kplc, med(dg), med(dk), med(do)))
        # how many placements simply have NO mesh at each level
        tot = len(rr)
        print('placements with NO mesh at that level (absent):')
        for k, lvl in enumerate((4, 8, 16, 32)):
            n = sum(c for b, c in bs.items() if not (b in bases and bases[b]['slots'][k]))
            print('  LOD%-2d  %7d of %7d  (%.1f%%)' % (lvl, n, tot, 100.0 * n / tot))

    # ---------------------------------------------------- section 2: far-only bases
    print('\n' + '=' * 78)
    print('SECTION 2 -- bases whose ONLY filled MNAM slots are FAR ones')
    pat = collections.Counter()
    faronly = []
    for f, v in bases.items():
        s = v['slots']
        if not any(s):
            continue
        filled = tuple(i for i in range(4) if s[i])
        pat[filled] += 1
        if filled and min(filled) >= 2:
            faronly.append(f)
    print('fill patterns over the %d bases in Fallout4.esm that have any MNAM slot:'
          % sum(pat.values()))
    for k, n in pat.most_common():
        print('  slots %-14s %6d' % (str(k), n))
    print('\nfar-only (first filled slot >= 2): %d bases' % len(faronly))
    placed = [f for f in faronly if cw.get(f, 0) > 0]
    print('of those, placed in the Commonwealth: %d' % len(placed))
    rows = []
    for f in faronly:
        v = bases[f]
        k = min(i for i in range(4) if v['slots'][i])
        m = measure(v['slots'][k])
        rows.append((obnd_diag(v['obnd']), f, v, k, m, cw.get(f, 0)))
    rows.sort(reverse=True)
    print('\nten largest far-only bases (OBND diag | formID | editorID | slots | '
          'mesh diag | tris | Commonwealth placements):')
    for dg, f, v, k, m, n in rows[:10]:
        print('  %8.0f  %08X  %-38s slots=%s  meshdiag=%s tris=%s  refs=%d'
              % (dg, f, v['edid'][:38],
                 ''.join('%d' % i if v['slots'][i] else '.' for i in range(4)),
                 ('%.0f' % m[3]) if m else '?', (m[0] if m else '?'), n))
        for i in range(4):
            if v['slots'][i]:
                print('        slot %d: %s' % (i, v['slots'][i]))

    # bases whose slot-2/3 mesh is BIGGER than the median -- the "whole building" test
    print('\n' + '=' * 78)
    print('SECTION 2b -- the largest meshes that exist AT ALL in slot 2 and slot 3,')
    print('             over every base placed in the Commonwealth')
    for k, lvl in ((2, 16), (3, 32)):
        cand = []
        for f, v in bases.items():
            if not v['slots'][k] or cw.get(f, 0) == 0:
                continue
            m = measure(v['slots'][k])
            if m:
                cand.append((m[3], f, v, m))
        cand.sort(reverse=True)
        print('\nLOD%-2d  %d placed bases carry a slot-%d mesh; ten largest:' % (lvl, len(cand), k))
        for dg, f, v, m in cand[:10]:
            print('  meshdiag %8.0f tris %5d  %08X %-34s  refs %5d  %s'
                  % (dg, m[0], f, v['edid'][:34], cw.get(f, 0), v['slots'][k]))
        if cand:
            print('  median mesh diag at this level, placed bases: %.0f' % med([c[0] for c in cand]))

    # ------------------------------------------- name-pattern search for whole buildings
    print('\n' + '=' * 78)
    print('SECTION 2c -- name-pattern search: bases whose editor ID looks like a whole')
    print('             building (Bldg*, *Building*, *Tower*, *_LOD*) and their slots')
    import re
    rx = re.compile(r'(bldg|building|tower|skyscraper|highrise)', re.I)
    hits = [(f, v) for f, v in bases.items()
            if rx.search(v['edid'] or '') and any(v['slots']) and cw.get(f, 0) > 0]
    pat2 = collections.Counter()
    for f, v in hits:
        pat2[tuple(i for i in range(4) if v['slots'][i])] += 1
    print('  %d placed bases match; their slot-fill patterns:' % len(hits))
    for k, n in pat2.most_common():
        print('    slots %-14s %5d' % (str(k), n))
    big = []
    for f, v in hits:
        m = measure(v['slots'][min(i for i in range(4) if v['slots'][i])])
        if m:
            big.append((m[3], f, v, m))
    big.sort(reverse=True)
    for dg, f, v, m in big[:10]:
        print('    meshdiag %8.0f tris %5d  %08X %-34s slots=%s refs %d'
              % (dg, m[0], f, v['edid'][:34],
                 ''.join('%d' % i if v['slots'][i] else '.' for i in range(4)), cw.get(f, 0)))


if __name__ == '__main__':
    main()
