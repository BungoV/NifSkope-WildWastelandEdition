"""Two alternatives to colour matching, measured on the same OUTWARD holdout.

A. GEOGRAPHIC EXTENSION.  Forget the colour: give each unpainted quadrant the
   material of the nearest PAINTED quadrant.  If this beats colour matching then
   the vanilla LOD pixels carry less information than mere proximity does, which
   would be a significant thing to know before shipping a colour-based mod.

B. COARSE FAMILIES.  74 materials is a hopeless number of classes for the
   signal available.  A texture replacer arguably only needs the right FAMILY --
   sand vs rubble vs grass vs mud vs ocean floor -- since it is replacing a
   whole coherent texture set.  Families are cut from the EDID name, which is
   Bethesda's own grouping and owes nothing to our colour model.  This measures
   what accuracy is available if the deliverable is deliberately coarsened.

C. The combination: colour matching restricted to materials that actually occur
   near the target cell.
"""
import os
import re
import sys
from collections import Counter

import numpy as np

from rec_common import HERE, MINX, MINY, N, parse_layers, ltex_names
from rec_holdout import TRANSFER, load, knn_predict, folds_outward

FAMILY = [
    ('ocean', r'oceanfloor'),
    ('coast-sand', r'coastsand|coastgrasssand'),
    ('coast-rock', r'coastwetrock'),
    ('riverbed-rock', r'riverbedrocks'),
    ('riverbed-silt', r'riverbedsilt'),
    ('marsh', r'marsh'),
    ('glowingsea', r'glowingsea'),
    ('blastedforest', r'blastedforest'),
    ('nfoothills', r'nfoothills'),
    ('nf-farharbor', r'^lnf_'),
    ('grass-dry', r'driedgrass|wildgrass|greenlawn|scrubgrass'),
    ('forest-floor', r'forestfloor|fallenleaves|rootseroded|muddyleaves'),
    ('rubble-debris', r'rubble|debris|crater|trash'),
    ('dirt-gravel', r'dirtgravel|dirtpath|prewardirt|crackedmud|asphalt|cobble'),
    ('none', r'^<no btxt'),
]


def family_of(edid):
    e = (edid or '').lower()
    for nm, pat in FAMILY:
        if re.search(pat, e):
            return nm
    return 'other'


def main():
    Feat, dom, XY, W, ltex = load()
    names = ltex_names()
    fam = {int(f): family_of(names.get(int(f), ('',))[0]) for f in np.unique(dom)}
    fams = sorted(set(fam.values()))
    print('materials %d -> families %d: %s' % (len(fam), len(fams), ', '.join(fams)))

    f = folds_outward(XY)
    te, tr = (f == 1), (f == 0)
    truth = dom[te]
    trXY, teXY = XY[tr], XY[te]

    # --- A. geographic extension: nearest training quadrant by cell distance ---
    print('\n=== A. geographic extension (nearest painted quadrant, no colour) ===')
    tp = trXY[:, :2].astype(np.float32)
    predA = np.empty(len(teXY), dtype=np.int64)
    for s in range(0, len(teXY), 512):
        e = min(s + 512, len(teXY))
        d = np.linalg.norm(teXY[s:e, None, :2].astype(np.float32) - tp[None, :, :], axis=2)
        # tie-break among all quadrants at the minimum distance by majority vote
        idx = np.argsort(d, axis=1)[:, :8]
        for r in range(e - s):
            predA[s + r] = Counter(dom[tr][idx[r]].tolist()).most_common(1)[0][0]
    accA = float((predA == truth).mean())
    print('  LTEX top-1 %.1f%%   (colour+ scores 28.4%% on the same split)' % (100 * accA))
    fa = np.array([fam[int(p)] for p in predA])
    ft = np.array([fam[int(t)] for t in truth])
    print('  family top-1 %.1f%%' % (100 * (fa == ft).mean()))

    # --- B. coarse families with the colour model ---
    print('\n=== B. coarse families, colour+ model ===')
    mu, sd = Feat.mean(0), Feat.std(0) + 1e-6
    Xtr = (Feat[tr][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    Xte = (Feat[te][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER]
    pred, conf = knn_predict(Xtr, dom[tr], Xte)
    pf = np.array([fam[int(p)] for p in pred])
    tf = np.array([fam[int(t)] for t in truth])
    print('  family top-1 %.1f%%' % (100 * (pf == tf).mean()))
    base = Counter(tf.tolist()).most_common(1)[0]
    print('  null (always "%s"): %.1f%%' % (base[0], 100.0 * base[1] / len(tf)))
    print('  per-family recall:')
    for nm in fams:
        m = (tf == nm)
        if m.sum() < 20:
            continue
        wrong = Counter(pf[m & (pf != tf)].tolist()).most_common(1)
        print('    %-16s n=%5d  recall %5.1f%%   mostly called: %s'
              % (nm, m.sum(), 100 * (pf[m] == nm).mean(),
                 wrong[0][0] if wrong else '-'))

    # --- C. colour restricted to geographically plausible materials ---
    print('\n=== C. colour+, but only allowed to choose materials that occur '
          'within 12 cells ===')
    predC = np.empty(len(teXY), dtype=np.int64)
    for i, (cx, cy, q) in enumerate(teXY):
        near = np.abs(trXY[:, 0] - cx) <= 12
        near &= np.abs(trXY[:, 1] - cy) <= 12
        if near.sum() < 20:
            predC[i] = pred[i]
            continue
        sub = np.where(tr)[0][near]
        d = np.linalg.norm(Xte[i] - (Feat[sub][:, TRANSFER] - mu[TRANSFER]) / sd[TRANSFER], axis=1)
        k = min(15, len(sub))
        idx = np.argpartition(d, k - 1)[:k]
        w = 1.0 / (d[idx] + 1e-3)
        acc = {}
        for j, ii in enumerate(idx):
            acc[dom[sub[ii]]] = acc.get(dom[sub[ii]], 0.0) + w[j]
        predC[i] = max(acc.items(), key=lambda kv: kv[1])[0]
    accC = float((predC == truth).mean())
    pcf = np.array([fam[int(p)] for p in predC])
    print('  LTEX top-1 %.1f%%   family top-1 %.1f%%'
          % (100 * accC, 100 * (pcf == tf).mean()))


if __name__ == '__main__':
    sys.exit(main())
