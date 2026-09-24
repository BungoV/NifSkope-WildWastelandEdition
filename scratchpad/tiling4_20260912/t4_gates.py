"""TILING4 -- the DECIDED gates, in one place, so selection and validation are
graded by the same code.

The director's decision of 2026-09-12 01:4x (brief_tiling4b.md) replaces the
brief's literal "grain within 20 % of the same chunk's vanilla, per sheet" and
its band-table twin, for the reason measured in logs/g0_grain.txt: vanilla's own
grain over these sheets spans a factor of 5.1 and its local variance a factor of
23.8, set by what terrain is there, and a change to the SAMPLER cannot move one
sheet's grain to that sheet's vanilla value.  The literal counts are still
computed and reported by every caller.

  G1       median hp SD over the set within 20 % of vanilla's median over the
           SAME set.                                  (TILING3's own criterion)
  G2       per sheet, hp SD within 20 % of THE RUNG's hp SD on that sheet.
                                     (a per-sheet gate with a per-sheet floor)
  G1-band  each of the six radial bands: the median share over the set within
           20 % of vanilla's median share.  Pass = 6 of 6; 5 of 6 (a5_tune's
           criterion) reported beside.
  G2-band  per sheet, the mean over the six bands of |share/vanilla - 1| no
           worse than THE RUNG's same number on that sheet.

  repeat   unchanged, per sheet: amplitude <= 0.264 (or the sheet's own
           no-repeat control where that reads higher) AND ratio <= 0.448.
  swirl    unchanged, per sheet: r <= 1.20 x the same chunk's vanilla r and
           r <= 3.2846.  Frozen 00:27.
"""
import statistics as st

MARG = 0.20


def band_scalar(bands, van_bands):
    e = [abs(b / max(g, 1e-12) - 1.0) for b, g in zip(bands, van_bands)]
    return sum(e) / len(e), max(e)


def decided(per, rung_per, van, sheets):
    """per / rung_per: {sheet: score dict}; van: {sheet: {'hpsd','bands'}}."""
    n = len(sheets)
    hp = [per[k]['hpsd'] for k in sheets]
    med = st.median(hp)
    med_van = st.median([van[k]['hpsd'] for k in sheets])
    g1_err = med / med_van - 1.0

    g2 = [abs(per[k]['hpsd'] / rung_per[k]['hpsd'] - 1.0) for k in sheets]
    g2_n = sum(1 for e in g2 if e <= MARG)

    nb = len(per[sheets[0]]['bands'])
    g1b = []
    for i in range(nb):
        m = st.median([per[k]['bands'][i] for k in sheets])
        mv = st.median([van[k]['bands'][i] for k in sheets])
        g1b.append(m / max(mv, 1e-12) - 1.0)
    g1b_n = sum(1 for e in g1b if abs(e) <= MARG)

    g2b, g2b_mx = [], []
    for k in sheets:
        me, mx = band_scalar(per[k]['bands'], van[k]['bands'])
        rme, rmx = band_scalar(rung_per[k]['bands'], van[k]['bands'])
        g2b.append(me <= rme + 1e-12)
        g2b_mx.append(mx <= rmx + 1e-12)
    g2b_n = sum(1 for ok in g2b if ok)

    rep = sum(1 for k in sheets if per[k]['rep_ok'])
    swl = sum(1 for k in sheets if per[k]['swirl_ok'])
    return dict(n=n, rep=rep, swirl=swl,
                g1_err=g1_err, g1_ok=bool(abs(g1_err) <= MARG),
                g2_n=g2_n, g2_worst=max(g2),
                g1b=g1b, g1b_n=g1b_n, g1b5=bool(g1b_n >= nb - 1),
                g2b_n=g2b_n, g2b_max_n=sum(1 for ok in g2b_mx if ok),
                lit_grain=sum(1 for k in sheets if per[k]['grain_ok']),
                lit_band=sum(1 for k in sheets if per[k]['band_ok']),
                med_hp=med, med_van_hp=med_van,
                rep_max=max(per[k]['vis'] for k in sheets),
                rat_max=max(per[k]['ratio'] for k in sheets),
                swirl_max=max(per[k]['swirl_r'] for k in sheets),
                all_ok=bool(rep == n and swl == n and abs(g1_err) <= MARG
                            and g2_n == n and g1b_n == nb and g2b_n == n),
                # the same verdict with G1-band dropped -- see logs/h3_sweep.txt:
                # the RUNG itself reads 2 of 6 and no sampler can move bands 3-5,
                # so G1-band is not a gate on the sampler either.
                all_ok_nog1b=bool(rep == n and swl == n and abs(g1_err) <= MARG
                                  and g2_n == n and g2b_n == n))


HEAD = ('   %-26s %5s | %4s %4s | %7s %4s | %5s %4s | %4s %4s | %4s'
        % ('variant', 'bias', 'rep', 'swl', 'G1', 'G2', 'G1bnd', 'G2bd',
           'litG', 'litB', 'ALL'))


def row(name, bias, r, key='all_ok_nog1b'):
    return ('   %-26s %5s | %4s %4s | %+6.1f%% %4s | %5s %4s | %4s %4s | %4s'
            % (name, ('%5.2f' % bias) if bias is not None else '   --',
               '%d/%d' % (r['rep'], r['n']), '%d/%d' % (r['swirl'], r['n']),
               100 * r['g1_err'], '%d/%d' % (r['g2_n'], r['n']),
               '%d/6' % r['g1b_n'], '%d/%d' % (r['g2b_n'], r['n']),
               '%d/%d' % (r['lit_grain'], r['n']), '%d/%d' % (r['lit_band'], r['n']),
               'PASS' if r[key] else '-'))
