#!/usr/bin/env python3
"""THE PROOF. The same three witnesses, re-read on the sheet the FIXED exe baked.

Nothing here re-derives a truth: TRUE / TRUEX / TER come out of table.json and
lit.json exactly as section 1 wrote them, computed from the raw BTD heightmap and
the .lodi/.lodo placement boxes, which the fix cannot touch. The only thing that
changed is which .lodt is read.

It also runs the control the candidate deserves: the C++ that shipped must
reproduce, inside the 0.353 deg quantisation step, the rule candidate.py measured
in Python before a line of C++ was written (cand_bins.json).
"""
import json
import math
import sys
import numpy as np

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/horizon2_20260918'
sys.path.insert(0, L)
from wit import true_skyline, LANE
from sheet import HorizonSheet
import fields

A = 16
AZ = np.arange(0.0, 360.0, 1.0)
AZB = np.array([b * (360.0 / A) for b in range(A)])
SECT = [np.array([i for i, a in enumerate(AZ)
                  if min((a - b * 22.5) % 360.0, (b * 22.5 - a) % 360.0) <= 11.25 + 1e-9])
        for b in range(A)]
OLD = LANE.replace('horizon2_20260918', 'horizon1_20260918') + '/v8/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt'
NEW = L + '/v8/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.4.lodt'
hs_old, hs_new = HorizonSheet(OLD), HorizonSheet(NEW)

land, ground, sky, ter0 = fields.build(verbose=False)
B = np.load(L + '/boxes.npy')
rows = json.load(open(L + '/table.json'))

# ---------------------------------------------------------------- receivers
D = np.array([true_skyline(land, r['x'], r['y'], r['gzLattice'] + 4.0, AZB, boxes=B,
                           skip_containing=bool(r['inBoxes'])) for r in rows])
T = np.array([r['TRUE'] for r in rows])
TX = np.array([r['TRUEX'] for r in rows])
U = np.where(np.array([[r['inBoxes']] for r in rows]) > 0, TX, T)
S_old = np.array([r['STORED'] for r in rows])
S_new = np.array([[float(v) * 90.0 / 255.0 for v in hs_new.bins_at(r['x'], r['y'])[0]] for r in rows])
CAND = np.array(json.load(open(L + '/cand_bins.json'))) if False else None

print('=== THE TEN RECEIVERS, 160 stored bins, degrees ===')
print('%-34s %8s %8s %8s %8s' % ('', 'mean', 'max', '>2deg', 'bias'))
for nm, a, t in (('OLD sheet vs DIRECTIONAL truth', S_old, D),
                 ('NEW sheet vs DIRECTIONAL truth', S_new, D),
                 ('OLD sheet vs SECTOR-MAX truth', S_old, U),
                 ('NEW sheet vs SECTOR-MAX truth', S_new, U)):
    e = np.abs(a - t)
    print('%-34s %8.2f %8.2f %7.1f%% %+8.2f' % (nm, e.mean(), e.max(), 100.0 * (e > 2).mean(), (a - t).mean()))
print()
hdr = 'bin'.rjust(7) + ''.join('%7d' % b for b in range(A))
for i, r in enumerate(rows):
    print('=== %-5s %-4s (%.0f, %.0f)' % (r['id'], r['class'], r['x'], r['y']))
    print(hdr)
    for nm, v in (('TRUEdir', D[i]), ('OLD', S_old[i]), ('NEW', S_new[i]), ('TER', r['TER'])):
        print('%7s' % nm + ''.join('%7.1f' % q for q in v))
    print('%7s mean |OLD-TRUEdir| %5.2f -> |NEW-TRUEdir| %5.2f   bias %+5.2f -> %+5.2f'
          % ('', np.abs(S_old[i] - D[i]).mean(), np.abs(S_new[i] - D[i]).mean(),
             (S_old[i] - D[i]).mean(), (S_new[i] - D[i]).mean()))

# ---------------------------------------------------------------- the lit question
rec = json.load(open(L + '/lit.json'))
cand = json.load(open(L + '/cand_bins.json'))


def sheet_elev(bins, az, scale):
    f = az / (360.0 / A)
    k0 = int(math.floor(f)) % A
    k1 = (k0 + 1) % A
    t = f - math.floor(f)
    return (bins[k0] * (1.0 - t) + bins[k1] * t) * scale


def bal(a, t):
    tp = float((a & t).sum()); fn = float((~a & t).sum())
    tn = float((~a & ~t).sum()); fp = float((a & ~t).sum())
    sens = tp / (tp + fn) if tp + fn else 1.0
    spec = tn / (tn + fp) if tn + fp else 1.0
    return 0.5 * (sens + spec)


NEWB = []
for r in rec:
    g = hs_new.bins_at(r['x'], r['y'])
    NEWB.append(list(g[0]))
json.dump(NEWB, open(L + '/new_bins.json', 'w'))

# CONTROL: the shipped C++ against the Python candidate it was written from
cp = np.array([[sheet_elev(c, az, 1.0) for az in (120.0, 240.0)] for c in cand])
np_ = np.array([[sheet_elev(b, az, 90.0 / 255.0) for az in (120.0, 240.0)] for b in NEWB])
print()
print('CONTROL  the shipped C++ vs the Python candidate it was written from:')
print('         |C++ - candidate| over %d texels x 2 azimuths: mean %.3f deg, max %.3f deg'
      ' (one stored step is %.3f deg)' % (len(rec), np.abs(np_ - cp).mean(), np.abs(np_ - cp).max(), 90.0 / 255.0))

print()
print('=== THE LIT QUESTION over %d real texels of chunk 4.4.-12 ===' % len(rec))
print('%-13s %7s %7s %7s %7s    %s' % ('sun', 'TRUE', 'OLD', 'REF', 'NEW', 'balanced agreement with TRUE'))
worst_old = worst_new = 100.0
for az in (120.0, 240.0):
    TT = np.array([r['true%d' % int(az)] for r in rec])
    SO = np.array([r['sheet%d' % int(az)] for r in rec])
    RF = np.array([r['ref%d' % int(az)] for r in rec])
    SN = np.array([sheet_elev(b, az, 90.0 / 255.0) for b in NEWB])
    for el in (5.0, 15.0, 30.0, 60.0):
        t = TT < el
        o, n, rr = SO < el, SN < el, RF < el
        bo, bn = 100.0 * bal(o, t), 100.0 * bal(n, t)
        worst_old, worst_new = min(worst_old, bo), min(worst_new, bn)
        print('az %3.0f el %2.0f   %6.1f%% %6.1f%% %6.1f%% %6.1f%%    old %5.1f%%  ref %5.1f%%  NEW %5.1f%%'
              % (az, el, 100.0 * t.mean(), 100.0 * o.mean(), 100.0 * rr.mean(), 100.0 * n.mean(),
                 bo, 100.0 * bal(rr, t), bn))
    print('  az %3.0f elevation deg: TRUE %5.1f | OLD %5.1f (err %5.2f bias %+5.2f) | NEW %5.1f (err %5.2f bias %+5.2f)'
          % (az, TT.mean(), SO.mean(), np.abs(SO - TT).mean(), (SO - TT).mean(),
             SN.mean(), np.abs(SN - TT).mean(), (SN - TT).mean()))
print()
print('WORST balanced agreement with the third witness: old %.1f%% -> new %.1f%%' % (worst_old, worst_new))
