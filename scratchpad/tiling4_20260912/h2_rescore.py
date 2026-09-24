"""TILING4 step 2b -- RE-SCORE every candidate of h1_sweep.json under the
director's decided grain and band gates (brief_tiling4b.md, 2026-09-12 01:4x),
which replace the brief's literal "within 20 % of the same chunk's vanilla, per
sheet" clause for the GRAIN and the BAND TABLE only.

Why the literal clause was refused is in logs/g0_grain.txt and is not re-argued
here: vanilla's own grain over these seven sheets spans a factor of 5.1 (hp SD
1.37 to 6.93) and 23.8 (local variance), set by which terrain is there, while
anything our compositor can produce spans a factor of 1.3 -- so a change to the
SAMPLER cannot move one sheet's grain to that sheet's vanilla value, and neither
the rung (1 of 7) nor TILING3's shipped proposal (4 of 7) meets it.

THE GATES AS DECIDED (registered here, in code, before this script was run):

  G1      the MEDIAN hp SD over the seven within 20 % of vanilla's median over
          THE SAME seven.  TILING3's own reported criterion.
  G2      PER SHEET, hp SD within 20 % of THE RUNG's hp SD on that same sheet:
          no grain regression from the sampler change.  A per-sheet gate with a
          per-sheet floor -- TILING3's mistake entry, answered.
  G1-band each of the six radial bands: the MEDIAN share over the seven within
          20 % of vanilla's median share over the same seven.  Pass = all six
          (5 of 6, TILING3's a5_tune criterion, is reported beside it).
  G2-band PER SHEET, the mean over the six bands of |share/vanilla - 1| no worse
          than THE RUNG's same number on that sheet.  ("No worse" = <=; the
          rung passes by construction, it IS the floor.)  The max-band version
          is reported beside it.

  repeat and swirl are UNCHANGED and stay per sheet with the ceilings frozen at
  00:11 / 00:27: repeat <= 0.264 absolute (or the sheet's own no-repeat control
  where that reads higher) and <= 0.448 over the sheet's own null floor; swirl
  r <= 1.20 x the same chunk's vanilla r and r <= 3.2846.

  The brief's LITERAL per-sheet-vs-vanilla counts are printed in every table so
  nothing is hidden by the change of gate.

No sheet is re-baked: every per-sheet number this needs is already in
h1_sweep.json (h1_sweep.py wrote the full per-sheet dict for all 28 variants,
the rung and TILING3's proposal).  Vanilla's own values are recovered exactly
from the errors that were stored against them:

    van_hp      = hpsd / (1 + grain_err)
    van_band[i] = bands[i] / (1 + band_err[i])

    python h2_rescore.py  ->  logs/h2_rescore.txt, h2_rescore.json
"""
import json
import os
import statistics as st

HERE = os.path.dirname(os.path.abspath(__file__))
MARG = 0.20

SW = json.load(open(os.path.join(HERE, 'h1_sweep.json')))
POOL = json.load(open(os.path.join(HERE, 'pool.json')))
SEL = ['%d,%d' % tuple(c) for c in POOL['selection']]


def van_of(per):
    """Vanilla's own hp SD and band shares per sheet, recovered from the errors."""
    out = {}
    for k, v in per.items():
        out[k] = dict(hp=v['hpsd'] / (1.0 + v['grain_err']),
                      bands=[b / (1.0 + e) for b, e in zip(v['bands'], v['band_err'])])
    return out


RUNG = SW['rung']['per']
VAN = van_of(RUNG)


def band_err_scalar(v, van):
    e = [abs(b / max(g, 1e-12) - 1.0) for b, g in zip(v['bands'], van['bands'])]
    return sum(e) / len(e), max(e)


def rescore(per, sheets=None):
    """Every decided gate on one variant's per-sheet dict."""
    sheets = sheets or SEL
    hp = [per[k]['hpsd'] for k in sheets]
    van_hp = [VAN[k]['hp'] for k in sheets]
    rung_hp = [RUNG[k]['hpsd'] for k in sheets]

    med, med_van = st.median(hp), st.median(van_hp)
    g1_err = med / med_van - 1.0
    g2 = [abs(h / r - 1.0) for h, r in zip(hp, rung_hp)]
    g2_n = sum(1 for e in g2 if e <= MARG)

    # G1-band: per band, the median share over the seven vs vanilla's median
    nb = len(per[sheets[0]]['bands'])
    g1b = []
    for i in range(nb):
        m = st.median([per[k]['bands'][i] for k in sheets])
        mv = st.median([VAN[k]['bands'][i] for k in sheets])
        g1b.append(m / max(mv, 1e-12) - 1.0)
    g1b_n = sum(1 for e in g1b if abs(e) <= MARG)

    # G2-band: per sheet, mean |band error| no worse than the rung's
    g2b, g2b_max = [], []
    for k in sheets:
        me, mx = band_err_scalar(per[k], VAN[k])
        rme, rmx = band_err_scalar(RUNG[k], VAN[k])
        g2b.append(me <= rme + 1e-12)
        g2b_max.append(mx <= rmx + 1e-12)
    g2b_n = sum(1 for ok in g2b if ok)

    rep = sum(1 for k in sheets if per[k]['rep_ok'])
    swl = sum(1 for k in sheets if per[k]['swirl_ok'])
    lit_g = sum(1 for k in sheets if per[k]['grain_ok'])     # the brief's literal
    lit_b = sum(1 for k in sheets if per[k]['band_ok'])
    n = len(sheets)
    allok = (rep == n and swl == n and abs(g1_err) <= MARG and g2_n == n
             and g1b_n == nb and g2b_n == n)
    return dict(n=n, rep=rep, swirl=swl, g1_err=g1_err, g1_ok=abs(g1_err) <= MARG,
                g2_n=g2_n, g2_worst=max(g2), g1b=g1b, g1b_n=g1b_n,
                g1b_worst=max(abs(e) for e in g1b), g2b_n=g2b_n,
                g2b_max_n=sum(1 for ok in g2b_max if ok),
                lit_grain=lit_g, lit_band=lit_b, all_ok=bool(allok),
                med_hp=med, med_van_hp=med_van,
                rep_max=max(per[k]['vis'] for k in sheets),
                rat_max=max(per[k]['ratio'] for k in sheets),
                swirl_max=max(per[k]['swirl_r'] for k in sheets))


def row(name, bias, r):
    return ('   %-24s %5s | %4s %4s | %+6.1f%% %4s | %5s %4s | %4s %4s | %5s'
            % (name, ('%5.2f' % bias) if bias is not None else '   --',
               '%d/%d' % (r['rep'], r['n']), '%d/%d' % (r['swirl'], r['n']),
               100 * r['g1_err'], '%d/%d' % (r['g2_n'], r['n']),
               '%d/6' % r['g1b_n'], '%d/%d' % (r['g2b_n'], r['n']),
               '%d/%d' % (r['lit_grain'], r['n']), '%d/%d' % (r['lit_band'], r['n']),
               'PASS' if r['all_ok'] else '-'))


HEAD = ('   %-24s %5s | %4s %4s | %7s %4s | %5s %4s | %4s %4s | %5s'
        % ('variant', 'bias', 'rep', 'swl', 'G1', 'G2', 'G1bnd', 'G2bd',
           'litG', 'litB', 'ALL'))


def main():
    L = ['TILING4 -- every candidate re-scored under the DECIDED grain/band gates',
         '',
         'rep/swl: per sheet, ceilings frozen 00:11 and 00:27, unchanged.',
         'G1  : median hp SD over the seven vs vanilla`s median over the same',
         '      seven; the printed number is the error, the ceiling is +-20 %.',
         'G2  : per sheet within 20 % of THE RUNG`s hp SD on that sheet.',
         'G1bnd: of the six bands, how many have their median share over the seven',
         '      inside 20 % of vanilla`s median share (pass = 6 of 6).',
         'G2bd: per sheet, mean |band error| no worse than the rung`s on that sheet.',
         'litG/litB: the brief`s LITERAL per-sheet-vs-vanilla counts, reported so',
         '      the change of gate hides nothing.  Neither the rung nor TILING3`s',
         '      proposal can meet them (1 of 7 and 4 of 7 on the grain).',
         '']
    out = {}

    L.append('THE RUNG AND TILING3`S PROPOSAL:')
    L.append(HEAD)
    for key, nm, b in (('rung', 'rung (footprint)', 0.0),
                       ('proposal', 'TILING3 proposal', -1.0)):
        r = rescore(SW[key]['per'])
        out[nm] = r
        L.append(row(nm, b, r))
    L.append('')

    L.append('STAGE 1 -- the geometry at bias -1.00, and STAGE 2 -- the bias sweep:')
    L.append(HEAD)
    seen = set()
    rows = []
    for st_key in ('stage1', 'stage2'):
        for v in SW[st_key]:
            k = (v['name'], v['bias'])
            if k in seen:
                continue
            seen.add(k)
            r = rescore(v['per'])
            out['%s @ %.2f' % k] = r
            rows.append((v['name'], v['bias'], r))
    for nm, b, r in rows:
        L.append(row(nm, b, r))
    L.append('')

    green = [(nm, b, r) for nm, b, r in rows if r['all_ok']]
    if green:
        L.append('GATED 7 OF 7 UNDER THE DECIDED GATES: %d variant(s).' % len(green))
        for nm, b, r in green:
            L.append('   %s @ bias %.2f' % (nm, b))
    else:
        L.append('NOTHING GATES 7 OF 7 UNDER THE DECIDED GATES EITHER.')
        L.append('What is binding, counted over all %d variants:' % len(rows))
        for lab, f in (('repeat  (per sheet)', lambda r: r['rep'] == r['n']),
                       ('swirl   (per sheet)', lambda r: r['swirl'] == r['n']),
                       ('G1      (median grain)', lambda r: r['g1_ok']),
                       ('G2      (per sheet vs rung)', lambda r: r['g2_n'] == r['n']),
                       ('G1-band (six medians)', lambda r: r['g1b_n'] == 6),
                       ('G2-band (per sheet vs rung)', lambda r: r['g2b_n'] == r['n'])):
            L.append('   %-28s %2d of %d variants pass' % (lab, sum(1 for _, _, r in rows if f(r)), len(rows)))
        best = max(rows, key=lambda t: (t[2]['rep'] + t[2]['swirl'] + t[2]['g2_n']
                                        + t[2]['g2b_n'] + t[2]['g1b_n']
                                        + (1 if t[2]['g1_ok'] else 0)))
        L.append('   closest: %s @ %.2f' % (best[0], best[1]))
    L.append('')

    # ---- which SHEET fails the repeat, and by how much, on the best geometries
    L.append('THE REPEAT, SHEET BY SHEET, on the variants with the best repeat count:')
    L.append('   (every number beside the ceiling it is judged against)')
    ranked = sorted(rows, key=lambda t: (-t[2]['rep'], -t[2]['swirl']))[:4]
    for nm, b, r in ranked:
        per = next(v['per'] for k in ('stage1', 'stage2') for v in SW[k]
                   if v['name'] == nm and v['bias'] == b)
        L.append('   %s @ bias %.2f  (repeat %d of %d)' % (nm, b, r['rep'], r['n']))
        L.append('      %-12s %7s %7s %7s %7s %s'
                 % ('sheet', 'amp', 'absCeil', 'ratio', 'ratCeil', 'verdict'))
        for k in SEL:
            v = per[k]
            absceil = max(SW['abs_ceil'], v['vis'] / max(v['ratio'], 1e-12) * 0.0
                          + SW['abs_ceil'])
            # the sheet's own control ceiling, recovered: rep_ok needs vis<=ceil
            ceil_used = SW['abs_ceil']
            if not v['rep_ok'] and v['ratio'] <= SW['rat_ceil'] and v['vis'] > SW['abs_ceil']:
                ceil_used = SW['abs_ceil']
            L.append('      (%10s) %7.3f %7.3f %7.3f %7.3f %s'
                     % (k, v['vis'], ceil_used, v['ratio'], SW['rat_ceil'],
                        'ok' if v['rep_ok'] else 'REPEAT RED'))
    L.append('')

    txt = '\n'.join(L) + '\n'
    print(txt)
    with open(os.path.join(HERE, 'logs', 'h2_rescore.txt'), 'w', newline='\n') as f:
        f.write(txt)
    json.dump(out, open(os.path.join(HERE, 'h2_rescore.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
