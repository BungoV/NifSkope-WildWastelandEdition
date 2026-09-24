#!/usr/bin/env python3
"""LODLEVELS1 section 3 -- the plain answer, with its share.

At which level is a downtown building ONE object, and for what share of
downtown buildings does that hold?

The unit of "a building" here is a WHOLE-BUILDING LOD OBJECT: a base whose MNAM
mesh lives under `LOD\\Neighborhoods\\<district>\\`.  That is Bethesda's own
folder for the one-mesh-per-building LOD objects, and each one is placed
exactly once.  The denominator for "share" is stated three ways, because a
"building" has no record in the plugin and any single denominator would be an
opinion.
"""
import collections
import os
import pickle
import statistics
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ROOT + '/tests/spells')

CELL = 4096.0
DOWN = (0 * CELL, -16 * CELL, 12 * CELL, -4 * CELL)
CHUNK = (4 * CELL, -12 * CELL, 8 * CELL, -8 * CELL)


def isneigh(p):
    return 'neighborhoods' in (p or '').lower().replace('/', '\\').split('\\')


def iskit(modl, slots):
    p = (modl or slots[0] or '').lower().replace('/', '\\')
    for c in p.split('\\')[:-1]:
        if c in ('architecture', 'buildings') or c.endswith('kit'):
            return True
    return False


def main():
    d = pickle.load(open(os.path.join(HERE, 'esm.pkl'), 'rb'))
    bases, refs = d['bases'], d['refs']

    whole = {}
    for f, v in bases.items():
        ks = [k for k in range(4) if isneigh(v['slots'][k])]
        if ks:
            whole[f] = ks

    print('=' * 78)
    print('WHOLE-BUILDING LOD OBJECTS (MNAM mesh under LOD\\Neighborhoods\\)')
    print('  bases in Fallout4.esm: %d' % len(whole))
    first = collections.Counter(min(v) for v in whole.values())
    for k in range(4):
        print('  first appear at slot %d (LOD%-2d): %4d  (%.1f%%)'
              % (k, (4, 8, 16, 32)[k], first.get(k, 0),
                 100.0 * first.get(k, 0) / len(whole)))

    for nm, box in (('chunk 4.4.-12', CHUNK), ('downtown box x0..11 y-16..-5', DOWN)):
        x0, y0, x1, y1 = box
        rr = [r for r in refs if x0 <= r[2] < x1 and y0 <= r[3] < y1]
        print('\n' + '=' * 78)
        print('REGION %s -- %d placements' % (nm, len(rr)))
        print('-' * 78)
        print('level | drawn placements | of them whole-building | of them kit piece | other')
        for k, lvl in enumerate((4, 8, 16, 32)):
            drawn = [r for r in rr if bases.get(r[1]) and bases[r[1]]['slots'][k]]
            wb = [r for r in drawn if isneigh(bases[r[1]]['slots'][k])]
            kp = [r for r in drawn
                  if not isneigh(bases[r[1]]['slots'][k])
                  and iskit(bases[r[1]]['modl'], bases[r[1]]['slots'])]
            ot = len(drawn) - len(wb) - len(kp)
            print(' LOD%-2d | %16d | %22d | %17d | %5d'
                  % (lvl, len(drawn), len(wb), len(kp), ot))
        wb_any = set(r[0] for r in rr if r[1] in whole)
        print('\nwhole-building LOD placements present in this region at ANY level: %d'
              % len(wb_any))
        for k, lvl in enumerate((4, 8, 16, 32)):
            n = len(set(r[0] for r in rr if r[1] in whole and bases[r[1]]['slots'][k]
                        and isneigh(bases[r[1]]['slots'][k])))
            print('   of those, drawn at LOD%-2d: %4d  (%.1f%% of the region\'s '
                  'whole-building objects)' % (lvl, n, 100.0 * n / max(len(wb_any), 1)))

        # THE SHARE QUESTION, three denominators
        print('\nSHARE: at LOD16, is a downtown building one object?')
        k = 2
        drawn16 = [r for r in rr if bases.get(r[1]) and bases[r[1]]['slots'][k]]
        wb16 = [r for r in drawn16 if isneigh(bases[r[1]]['slots'][k])]
        print('  (i)  of the %d placements STILL DRAWN at LOD16, %d (%.1f%%) are '
              'whole-building objects' % (len(drawn16), len(wb16),
                                          100.0 * len(wb16) / max(len(drawn16), 1)))
        # kit pieces that had a mesh at LOD8 and lose it at LOD16
        d8 = [r for r in rr if bases.get(r[1]) and bases[r[1]]['slots'][1]]
        gone = [r for r in d8 if not bases[r[1]]['slots'][2]]
        print('  (ii) of the %d placements drawn at LOD8, %d (%.1f%%) have no mesh '
              'at LOD16 at all' % (len(d8), len(gone), 100.0 * len(gone) / max(len(d8), 1)))
        # how much of the region's LOD8 kit-piece footprint is covered by a
        # whole-building object at LOD16 -- by 512-unit ground bucket
        def buckets(rs):
            return set((int(r[2] // 512), int(r[3] // 512)) for r in rs)
        b8 = buckets([r for r in d8 if iskit(bases[r[1]]['modl'], bases[r[1]]['slots'])])
        b16 = buckets(wb16)
        # widen the whole-building buckets by their own footprint
        wide = set()
        for r in wb16:
            v = bases[r[1]]
            o = v['obnd']
            hx = max(abs(o[3] - o[0]), abs(o[4] - o[1])) * 0.5 * r[8] if o else 1024.0
            cx, cy = int(r[2] // 512), int(r[3] // 512)
            n = int(hx // 512) + 1
            for dx in range(-n, n + 1):
                for dy in range(-n, n + 1):
                    wide.add((cx + dx, cy + dy))
        cov = len(b8 & wide)
        print('  (iii) of the %d ground buckets (512 u) that hold a KIT PIECE at LOD8, '
              '%d (%.1f%%) fall inside a whole-building LOD object\'s footprint at LOD16'
              % (len(b8), cov, 100.0 * cov / max(len(b8), 1)))

    # ---- the median bound at each level, placements not bases (region = downtown)
    print('\n' + '=' * 78)
    print('MEDIAN OBND DIAGONAL of the BASE of every placement drawn at each level')
    print('(downtown box; OBND is the near model\'s own bounding box, so this is '
          '"how big is the thing" independent of the LOD mesh)')
    x0, y0, x1, y1 = DOWN
    rr = [r for r in refs if x0 <= r[2] < x1 and y0 <= r[3] < y1]
    for k, lvl in enumerate((4, 8, 16, 32)):
        v = []
        for r in rr:
            b = bases.get(r[1])
            if not b or not b['slots'][k] or not b['obnd']:
                continue
            o = b['obnd']
            v.append(((o[3] - o[0]) ** 2 + (o[4] - o[1]) ** 2 + (o[5] - o[2]) ** 2) ** 0.5)
        print('  LOD%-2d  n=%6d  median %8.0f  p90 %8.0f  max %8.0f'
              % (lvl, len(v), statistics.median(v) if v else float('nan'),
                 (sorted(v)[int(len(v) * 0.9)] if v else float('nan')),
                 max(v) if v else float('nan')))


if __name__ == '__main__':
    main()
