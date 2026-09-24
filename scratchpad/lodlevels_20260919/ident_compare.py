#!/usr/bin/env python3
"""IDENT lane -- part 3: our bake's group table vs the best ESM-native grouping.

Our side  : Commonwealth.lodi v8, T['group'][i] (u16, dense per CHUNK), so the
            global key is (chunk index, group id).  T['cold'][i]['refFormId']
            joins it to the plugin; T['cold'][i]['scolPart'] says a SCOL
            instance was exploded, so several instances can share one refForm.
Their side: the REFR's XLYR layer (the only mechanism above 12% -- see
            ident_notes.md).  A placement with no XLYR is its OWN singleton,
            because "no layer" is not an identity and must not be allowed to
            merge everything unlayered into one giant class.

Metric: pair counting over the SAME placement set.
  a = pairs co-grouped by BOTH        b = ours only     c = theirs only
  d = separated by both               Rand = (a+d)/N2   Jaccard = a/(a+b+c)
Raw counts are printed, not only the ratios.
"""
import math
import os
import pickle
import struct
import sys
from collections import Counter, defaultdict

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
LANE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = ('C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/'
           '392777f8-9016-4913-858d-16d6eec4c01a/scratchpad')
LODI = (ROOT + '/scratchpad/horizon2_20260918/dumpbake/nat/FO4CSLOD/'
        'Commonwealth/Commonwealth.lodi')
sys.path.insert(0, ROOT + '/tests/spells')
sys.path.insert(0, LANE)
import lodgen_native_decode as D            # noqa: E402
from ident_measure import is_arch, diag      # noqa: E402


def pair_counts(A, B):
    """A, B: lists of labels over the same elements.  Returns a,b,c,d."""
    n = len(A)
    N2 = n * (n - 1) // 2
    ca = Counter(A)
    cb = Counter(B)
    cab = Counter(zip(A, B))
    sa = sum(v * (v - 1) // 2 for v in ca.values())
    sb = sum(v * (v - 1) // 2 for v in cb.values())
    a = sum(v * (v - 1) // 2 for v in cab.values())
    return a, sa - a, sb - a, N2 - sa - sb + a, N2


def main():
    T = D.read_lodi(LODI)
    E = pickle.load(open(os.path.join(LANE, 'esm.pkl'), 'rb'))
    S = pickle.load(open(os.path.join(SCRATCH, 'ident_scan.pkl'), 'rb'))
    bases, refsub, layr = E['bases'], S['refsub'], S['layr']
    refbase = {r[0]: r[1] for r in E['refs']}
    refpos = {r[0]: (r[2], r[3], r[4]) for r in E['refs']}
    out = []

    def say(s=''):
        out.append(s)
        print(s)

    h = T['header']
    say('lodi v%d  instances %d  chunks %d  groupCount %d'
        % (h['version'], h['instanceCount'], h['chunkCount'], h['groupCount']))

    chunkOf = [0] * h['instanceCount']
    for ci, c in enumerate(T['chunks']):
        for i in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
            chunkOf[i] = ci

    ours, theirs, forms, kinds = [], [], [], []
    nolayer = 0
    for i in range(h['instanceCount']):
        f = T['cold'][i]['refFormId']
        ours.append((chunkOf[i], T['group'][i]))
        s = refsub.get(f, {})
        if b'XLYR' in s:
            theirs.append(('L', struct.unpack_from('<I', s[b'XLYR'][0], 0)[0]))
        else:
            theirs.append(('single', i))
            nolayer += 1
        forms.append(f)
    say('placements %d, distinct refForms %d, SCOL-exploded placements %d'
        % (len(forms), len(set(forms)),
           sum(1 for i in range(h['instanceCount']) if T['cold'][i]['scolPart'] >= 0)))
    say('our groups %d (largest %d, singletons %d)'
        % (len(set(ours)), max(Counter(ours).values()),
           sum(1 for v in Counter(ours).values() if v == 1)))
    say('their layers %d over %d placements; %d placements carry NO layer '
        '(counted as singletons)'
        % (len(set(t for t in theirs if t[0] == 'L')),
           len(forms) - nolayer, nolayer))
    say()

    for label, idx in (('ALL placements', list(range(len(forms)))),
                       ('ARCHITECTURE placements only',
                        [i for i in range(len(forms))
                         if is_arch(bases.get(refbase.get(forms[i], 0), {}).get('modl', ''))])):
        A = [ours[i] for i in idx]
        B = [theirs[i] for i in idx]
        a, b, c, d, N2 = pair_counts(A, B)
        say('%s  (n=%d, pairs=%d)' % (label, len(idx), N2))
        say('  a co-grouped by BOTH        %10d' % a)
        say('  b co-grouped by OURS only   %10d   (we merge what the ESM separates)' % b)
        say('  c co-grouped by THEIRS only %10d   (we split what the ESM joins)' % c)
        say('  d separated by both         %10d' % d)
        say('  Rand agreement (a+d)/pairs  %8.4f' % ((a + d) / float(N2)))
        say('  Jaccard on co-grouped pairs %8.4f' % (a / float(max(1, a + b + c))))
        say()

    # ------------------------------------------- where they disagree, by layer
    say('DISAGREEMENTS, per layer (architecture placements):')
    byLayer = defaultdict(list)
    for i in range(len(forms)):
        if theirs[i][0] != 'L':
            continue
        byLayer[theirs[i][1]].append(i)
    rows = []
    for lf, idx in byLayer.items():
        if len(idx) < 8:
            continue
        og = Counter(ours[i] for i in idx)
        # how many of OUR groups this layer is cut into, and how much of our
        # biggest group leaks outside the layer
        leak = 0
        biggest = og.most_common(1)[0][0]
        allours = Counter(ours)
        leak = allours[biggest] - og[biggest]
        rows.append((len(idx), len(og), og.most_common(1)[0][1], leak,
                     layr.get(lf, ('?', 0))[0], lf, idx))
    rows.sort(key=lambda t: -t[0])
    say('  %-40s %6s %7s %6s %6s' % ('layer', 'plcmts', 'ourGrps', 'big', 'leak'))
    for n, ng, big, leak, ed, lf, idx in rows[:25]:
        say('  %-40s %6d %7d %6d %6d' % (ed[:40], n, ng, big, leak))
    say()

    # five named buildings where they disagree, with the direction
    say('FIVE NAMED DISAGREEMENTS:')
    picked = 0
    for n, ng, big, leak, ed, lf, idx in rows:
        if picked >= 5:
            break
        direction = ('WE SPLIT what the ESM joins' if ng > 1 and leak == 0 else
                     'WE MERGE what the ESM separates' if leak > 0 and ng == 1 else
                     'BOTH: we split this layer AND our groups reach outside it')
        if ng == 1 and leak == 0:
            continue
        pts = [refpos[forms[i]] for i in idx if forms[i] in refpos]
        say('  %-40s layer %08X  %d placements -> %d of our groups '
            '(biggest %d), %d foreign placements inside that group'
            % (ed, lf, n, ng, big, leak))
        say('      %s ; layer world diagonal %.0f units (%.1f m)'
            % (direction, diag(pts), diag(pts) * 0.0142875))
        top = Counter(bases.get(refbase.get(forms[i], 0), {}).get('edid', '?')
                      for i in idx).most_common(3)
        say('      commonest bases: %s' % ', '.join('%s x%d' % t for t in top))
        picked += 1

    with open(os.path.join(SCRATCH, 'ident_compare.txt'), 'w') as fh:
        fh.write('\n'.join(out))


if __name__ == '__main__':
    main()
