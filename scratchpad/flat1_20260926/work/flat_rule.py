"""FLAT1: apply a candidate flat rule to out/flat_census.pkl (read-only census of the Boston box) and print the
evidence: histograms, top models per kind by texels, top refused and why, the standing models, mod placements.

  python flat_rule.py [H] [S] [LOW] [HIGH]
    H     top of the object above the ground, 90th percentile over the 16-unit squares it covers (default 48)
    S     side area / top area (a thing that stands up has more side than top; default 1.0)
    LOW   the object's underside may sink this far (no limit by default; a sunk slab is still on the ground)
    HIGH  the underside (median over its squares) may float at most this far above the ground (default 16)
"""
import collections
import os
import pickle
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
D = pickle.load(open(os.path.join(HERE, 'out', 'flat_census.pkl'), 'rb'))
H = float(sys.argv[1]) if len(sys.argv) > 1 else 64.0
S = float(sys.argv[2]) if len(sys.argv) > 2 else 0.35
HIGH = float(sys.argv[3]) if len(sys.argv) > 3 else 16.0
BETH = {'fallout4.esm', 'dlcrobot.esm', 'dlcworkshop01.esm', 'dlccoast.esm', 'dlcworkshop02.esm',
        'dlcworkshop03.esm', 'dlcnukaworld.esm', 'dlcultrahighresolution.esm'}


def label(modl):
    p = modl.lower().replace(chr(92), '/')
    f = p.rsplit('/', 1)[-1]
    if 'rrtrack' in p or 'railroad/rrtie' in p or 'traintrack' in p or 'trackbed' in p:
        return 'rail'
    if 'decal' in f or 'crack' in f or 'stain' in f or 'manhole' in f or 'drain' in f or 'grate' in f:
        return 'decal'
    if any(k in p for k in ('parking', 'slab', 'foundation', 'floor', 'pad0', 'platform', 'lot0')):
        return 'pad'
    if any(k in p for k in ('path', 'trail', 'dirtroad', 'dirtpatch', 'mudpatch')) and 'trailer' not in p:
        return 'path'
    return 'debris'


def sidetop(r):
    """side area that rises more than 8 units above the ground / top area not buried more than 16"""
    return r['side'][8] / max(r['upvis'], 1.0)


def decide(r):
    """(decision, reason) for one measured placement. Kind-agnostic: no path is read."""
    if r['road']:
        return 'road', 'road stamp owns it'
    if r['disabled']:
        return 'refused', 'initially disabled'
    if r['status'] != 'measured' or not r.get('gsq'):
        return 'refused', r['status'] if r['status'] != 'measured' else 'no land'
    lo = float(np.median(r['glo']))
    hi = float(np.percentile(r['ghi'], 90))
    ratio = sidetop(r)
    if r['water'] is not None and r['pos'][2] < r['water']:
        return 'refused', 'under water'
    if lo > HIGH:
        return 'refused', 'not on the ground'
    if hi < -8:
        return 'refused', 'under the ground'
    if hi > H:
        return 'refused', 'too tall'
    if ratio > S:
        return 'refused', 'stands up'
    if r['upvis'] <= 0:
        return 'refused', 'no top surface'
    return 'painted', ''


def main():
    recs = D['recs']
    for r in recs:
        r['label'] = label(r['modl'])
        r['dec'], r['why'] = decide(r)
        r['tex'] = r.get('gsq', 0)
    M = [r for r in recs if r['status'] == 'measured' and not r['road'] and not r['disabled'] and r.get('gsq')]
    print('rule: top(p90) <= %.0f, side(>8)/top <= %.2f, underside(median) <= %.0f above ground, not under water' % (H, S, HIGH))
    print('placements examined', len(recs), 'measured', len(M))
    c = collections.Counter((r['dec'], r['why']) for r in recs)
    for k, v in c.most_common():
        print('  %-10s %-22s %6d' % (k[0], k[1], v))
    # histograms of the three measures over measured placements
    hi = np.array([np.percentile(r['ghi'], 90) for r in M])
    lo = np.array([np.median(r['glo']) for r in M])
    ra = np.array([sidetop(r) for r in M])
    tex = np.array([r['tex'] for r in M], float)
    print('top above ground, p90 (placements / squares), among underside <= %.0f:' % HIGH)
    e = [-1e9, 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 96, 128, 256, 1e9]
    on = lo <= HIGH
    h1, _ = np.histogram(hi[on], e)
    h2, _ = np.histogram(hi[on], e, weights=tex[on])
    h3, _ = np.histogram(hi[on & (ra <= S)], e)
    for i in range(len(h1)):
        print('  %6s..%-6s %6d placements %8.0f squares   of which side/top <= S: %6d' % (e[i], e[i + 1], h1[i], h2[i], h3[i]))
    print('side/top ratio (placements), among underside <= %.0f and top <= %.0f:' % (HIGH, H))
    e2 = [0, 0.05, 0.1, 0.2, 0.3, 0.35, 0.4, 0.5, 0.75, 1.0, 1.5, 2, 4, 1e9]
    h, _ = np.histogram(ra[on & (hi <= H)], e2)
    for i in range(len(h)):
        print('  %5s..%-5s %6d' % (e2[i], e2[i + 1], h[i]))
    print('underside above ground, median (placements):')
    e3 = [-1e9, -64, -16, -4, 0, 4, 8, 16, 32, 64, 128, 1e9]
    h, _ = np.histogram(lo, e3)
    for i in range(len(h)):
        print('  %6s..%-6s %6d' % (e3[i], e3[i + 1], h[i]))

    # per model
    agg = collections.defaultdict(list)
    for r in recs:
        if r['road']:
            continue
        agg[r['modl']].append(r)
    rows = []
    for m, rs in agg.items():
        p = [x for x in rs if x['dec'] == 'painted']
        rows.append((m, rs, p))
    print('\nTOP PAINTED per kind (by squares covered):')
    for k in ('pad', 'rail', 'path', 'decal', 'debris'):
        sel = sorted([t for t in rows if t[2] and t[2][0]['label'] == k], key=lambda t: -sum(x['tex'] for x in t[2]))
        print(' ', k, len(sel), 'models,', sum(len(t[2]) for t in sel), 'placements,',
              int(sum(sum(x['tex'] for x in t[2]) for t in sel)), 'squares')
        for m, rs, p in sel[:10]:
            print('    %6d sq  %4d/%-4d placed  top %5.1f  %s' % (sum(x['tex'] for x in p), len(p), len(rs),
                  np.median([np.percentile(x['ghi'], 90) for x in p]), m))
    print('\nTOP REFUSED (by squares, measured only):')
    ref = collections.defaultdict(lambda: [0, 0, collections.Counter(), []])
    for r in M:
        if r['dec'] == 'refused':
            a = ref[r['modl']]
            a[0] += r['tex']; a[1] += 1; a[2][r['why']] += 1; a[3].append(r)
    for m, a in sorted(ref.items(), key=lambda kv: -kv[1][0])[:25]:
        rs = a[3]
        print('  %7d sq %4d placed  %-18s top %6.1f under %6.1f side/top %5.2f  %s' % (
            a[0], a[1], a[2].most_common(1)[0][0], np.median([np.percentile(x['ghi'], 90) for x in rs]),
            np.median([np.median(x['glo']) for x in rs]), np.median([sidetop(x) for x in rs]), m))
    print('\nSTANDING MODELS:')
    for pat in ('guardrails', 'fencechainlink', 'picketfence', 'wroughtiron', 'jersey', 'barrier', 'vehicles/automotive',
                'carframe', 'retainingwall/residential/rwpiecewall', 'planter', 'bench', 'mailbox', 'hydrant'):
        sel = [r for r in M if pat in r['modl'].lower().replace(chr(92), '/')]
        if not sel:
            print('  %-40s none in the box' % pat)
            continue
        cc = collections.Counter(r['why'] if r['dec'] == 'refused' else r['dec'] for r in sel)
        print('  %-40s %4d placements  %s' % (pat, len(sel), dict(cc)))
        for r in sel:
            if r['dec'] == 'painted':
                print('       PAINTED %s top %.1f side/top %.2f' % (r['modl'], np.percentile(r['ghi'], 90), sidetop(r)))
    print('\nMODS (placements whose REFR comes from a non-Bethesda plugin):')
    mods = [r for r in recs if r['plugin'].lower() not in BETH]
    print('  examined', len(mods), 'measured', sum(1 for r in mods if r['status'] == 'measured'))
    cc = collections.Counter((r['plugin'], r['dec']) for r in mods)
    for k, v in sorted(cc.items()):
        print('   ', k, v)
    for r in mods:
        if r['dec'] == 'painted':
            print('    painted', r['plugin'], '%08X' % r['ref'], r['modl'], 'base plugin', r['baseplugin'])
    lod = [r for r in recs if r['dec'] == 'painted' and r['hasLod']]
    print('\nhas-LOD among painted:', len(lod), collections.Counter(r['modl'] for r in lod).most_common(8))
    pickle.dump([{k: r.get(k) for k in ('ref', 'part', 'plugin', 'modl', 'dec', 'why', 'label', 'tex')} for r in recs],
                open(os.path.join(HERE, 'out', 'flat_rule_py.pkl'), 'wb'))


if __name__ == '__main__':
    main()
