"""ROADS1: every road / pavement STAT base in the load order that the stamp refuses as raised, grouped by folder,
with its Commonwealth placement count (direct REFRs + SCOL parts) and the model's z extent.
  python haslod_census.py"""
import collections
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import roadgeo as rg  # noqa: E402


def main():
    R = rg.Reader()
    use = collections.Counter()
    scol_of = {}
    for form, e in R.W.bases.items():
        if e[4] == b'SCOL':
            bi = R.base_info(form)
            if bi:
                scol_of[form] = bi
    for form, r in R.W.refs.items():
        if r['flags'] & 0x20:
            continue
        b = r['base']
        if b in scol_of:
            for pb, pls in scol_of[b]['parts']:
                use[pb] += len(pls)
        else:
            use[b] += 1
    groups = collections.defaultdict(list)
    for form, e in R.W.bases.items():
        if e[4] != b'STAT':
            continue
        bi = R.base_info(form)
        if not bi or not rg.is_road(bi['modl']):
            continue
        if not (bi['hasLod'] or rg.is_raised_folder(bi['modl'])):
            continue
        c = rg.comps(bi['modl'])
        folder = '/'.join(c[:-1][-3:])
        m = R.model(bi['modl']) or []
        zs = [s['pos'][:, 2] for s in m if len(s['pos'])]
        z = (float(min(a.min() for a in zs)), float(max(a.max() for a in zs))) if zs else (0, 0)
        groups[folder].append((form, bi['modl'], bi['hasLod'], use[form], z))
    for folder in sorted(groups):
        rows = groups[folder]
        n = sum(r[3] for r in rows)
        print('%-40s bases %3d placed %5d  haslod %3d' % (folder, len(rows), n, sum(1 for r in rows if r[2])))
        for form, modl, hl, u, z in sorted(rows, key=lambda r: -r[3])[:4]:
            print('     %08X %-50s lod %d placed %4d z %.0f..%.0f' % (form, rg.comps(modl)[-1], hl, u, z[0], z[1]))


if __name__ == '__main__':
    main()
