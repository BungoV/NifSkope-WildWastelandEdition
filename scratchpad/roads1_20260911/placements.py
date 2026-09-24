"""Placed references in the Sanctuary window, SCOL parts expanded, each tagged
with the family test lane ROADS1 states in its report section 1.

THE ROAD TEST (stated here once, never a bare substring -- MISTAKES.md's
"sTREEt" lesson, and `setdressing/railROADs/waxcandle02off.nif` is this
region's live counter-example): a placement is a ROAD piece when

    * its base record's signature is STAT, and
    * its model path, separators normalised, lowercased, a leading `meshes`
      component dropped, has `landscape` as its first component and `roads`
      as its second.

Component equality, both components, in order.  `setdressing/railroad/...`
fails on the first component; `landscape/roads/sanctuary/...` passes.
"""

import json
import os
import sys

import numpy as np

from rasterlib import ref_matrix

SEP = chr(92)


def norm_components(modl):
    p = (modl or '').replace(SEP, '/').lower().lstrip('/')
    parts = [c for c in p.split('/') if c]
    if parts and parts[0] == 'meshes':
        parts = parts[1:]
    return parts


def is_road(sig, modl):
    c = norm_components(modl)
    return sig == 'STAT' and len(c) >= 3 and c[0] == 'landscape' and c[1] == 'roads'


def family(sig, modl):
    c = norm_components(modl)
    if is_road(sig, modl):
        return 'road'
    if len(c) >= 2 and c[0] == 'landscape':
        return 'landscape/' + c[1]
    if c:
        return c[0]
    return 'nomodel'


def load(path):
    """[(base formid, model, sig, pos(3), R(3x3), scale, ref, part, family)]"""
    d = json.load(open(path))
    bases = d['bases']
    out = []
    for r in d['refs']:
        key = '%08X' % r['base']
        rec = bases.get(key)
        if rec is None:
            continue
        R = ref_matrix(r['rot'])
        pos = np.array(r['pos'], dtype=np.float64)
        if rec['sig'] == 'SCOL':
            n = 0
            for p in rec['parts']:
                pk = '%08X' % p['base']
                prec = bases.get(pk)
                if prec is None:
                    continue
                for xf in p['xforms']:
                    pm = ref_matrix(xf[3:6])
                    ppos = pos + R.dot(np.array(xf[0:3]) * r['scale'])
                    out.append(dict(base=p['base'], modl=prec['modl'],
                                    sig=prec['sig'], pos=ppos, R=R.dot(pm),
                                    scale=r['scale'] * xf[6], ref=r['formid'],
                                    part=n, edid=prec['edid'],
                                    family=family(prec['sig'], prec['modl'])))
                    n += 1
            continue
        out.append(dict(base=r['base'], modl=rec['modl'], sig=rec['sig'],
                        pos=pos, R=R, scale=r['scale'], ref=r['formid'],
                        part=-1, edid=rec['edid'],
                        family=family(rec['sig'], rec['modl'])))
    return out


if __name__ == '__main__':
    import collections
    pl = load(sys.argv[1])
    c = collections.Counter(p['family'] for p in pl)
    print('placements (SCOL expanded): %d' % len(pl))
    for k, v in c.most_common(20):
        print('%6d %s' % (v, k))
    roads = [p for p in pl if p['family'] == 'road']
    print('road pieces: %d, distinct models %d'
          % (len(roads), len(set(p['modl'].lower() for p in roads))))
    for m in sorted(set(p['modl'].lower() for p in roads)):
        print('   ', m)
