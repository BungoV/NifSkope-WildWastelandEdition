#!/usr/bin/env python3
"""bridge_variants.py -- three candidate bridge rules, measured on the same grid.

bridge_effect.py showed rule D's bridge, with an EXACT shore test, hands the
Commonwealth's ocean to `ExtMarshScumWater`: 21,587,443 texels assembled from
332 components across five forms.  Two separate defects produce that:

  1. the bridge accepts "one side inherits" in BOTH directions, so a 21.5 M
     inheriting sea absorbs a painted marsh that comes within two texels of it;
  2. the merged body's form is "the most common PAINTED type", which ignores
     the inherited area entirely -- so a body that is 99.9% default takes the
     name of the 0.1% that was painted.

Variants measured here, all with the exact shore test:

  V0  as WATER1 states rule D                     (accept either direction)
  V1  V0 + the form is the majority type INCLUDING the worldspace default
  V2  V1 + the bridge merges an inheriting component INTO a painted one only
          when the inheriting side is the SMALLER of the two
"""

import collections
import os
import sys

import numpy as np

W1 = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  '..', 'water_20260909'))
sys.path.insert(0, W1)

import ccl                      # noqa: E402
import bridge_exact as BX       # noqa: E402

DEF = 0xFFFF


def run(info, pairs, keys, asymmetric, form_counts_default):
    uf = ccl.UF(max(keys) + 2)
    accepted = refused = 0
    for (a, b) in sorted(pairs):
        A, Bb = info[a], info[b]
        if abs(A['h'] - Bb['h']) > 0.01:
            continue
        if A['typeIdx'] == Bb['typeIdx']:
            uf.union(a, b)
            accepted += 1
            continue
        inhA, inhB = A['inherits'], Bb['inherits']
        if inhA or inhB:
            if not asymmetric:
                uf.union(a, b)
                accepted += 1
            else:
                # the inheriting side is absorbed, and only when it is smaller
                small = A if inhA else Bb
                big = Bb if inhA else A
                if inhA and inhB:
                    uf.union(a, b)
                    accepted += 1
                elif small['area'] < big['area']:
                    uf.union(a, b)
                    accepted += 1
                else:
                    refused += 1
        else:
            refused += 1
    grp = {}
    for k in keys:
        grp.setdefault(uf.find(k), []).append(k)

    rows = []
    for r, parts in grp.items():
        area = sum(info[k]['area'] for k in parts)
        tc = collections.Counter()
        for k in parts:
            tc[info[k]['typeIdx']] += info[k]['area']
        if form_counts_default:
            ti = tc.most_common(1)[0][0]
        else:
            painted = [t for t in tc if t != DEF]
            ti = max(painted, key=lambda t: tc[t]) if painted else DEF
        edid = ('DEFAULT (ExtOceanWater)' if ti == DEF else
                next(info[k]['edid'] for k in parts if info[k]['typeIdx'] == ti))
        rows.append((area, edid, info[parts[0]]['h'], len(parts)))
    rows.sort(reverse=True)
    return accepted, refused, rows


def main():
    d, wet, blab, bodies = BX.load_blab()
    info = {int(k): v for k, v in bodies.items()}
    keys = sorted(info)
    pairs = BX.exact_pairs(blab.astype(np.int32), BX.GAP)
    print('rule-C bodies %d, exact pairs within %d texels %d'
          % (len(keys), BX.GAP, len(pairs)))
    for name, asym, incdef in (('V0 rule D as stated', False, False),
                               ('V1 + majority form counts the default', False, True),
                               ('V2 + inheriting side must be smaller', True, True)):
        a, r, rows = run(info, pairs, keys, asym, incdef)
        print()
        print('%s : %d bodies, %d merges accepted, %d refused'
              % (name, len(rows), a, r))
        print('  %12s %-28s %8s %6s' % ('area', 'form', 'height', 'parts'))
        for row in rows[:8]:
            print('  %12d %-28s %8.1f %6d' % (row[0], row[1][:28], row[2], row[3]))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
