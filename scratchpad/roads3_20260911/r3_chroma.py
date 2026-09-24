"""ROADS3 gate F1, part 3 -- the HUE named by measurement, the detail strength
with a floor that works, and the candidate law SIMULATED offline before a
single line of C++ is written.

Three questions, in order:

  1  Is vanilla's far road a different COLOUR from the ground it lies on, or
     only a different brightness?  Measured as the rise of each channel and of
     the two opponent axes over the surround, vanilla and ours side by side.
  2  Does vanilla keep any of the road diffuse's own detail?  The residual
     after the best wash, correlated with our full-detail bake's departure from
     its flat average, against a floor that is the SAME signal with its phase
     broken (`splatlib.phase_twin`) -- the translated floor in `r3_fit.py`
     returned NaN because a translated mask-shaped signal is all zeros on the
     mask, which is a dead floor and is recorded as a mistake.
  3  What would the candidate law DO?  The wash is applied to the rung's own
     sheets offline -- ground plus opacity toward the paint, the opacity chosen
     so the result sits `rise` levels above the local non-road ground -- and
     every F3 gate is read off the simulation.  Nothing is built until this
     says the gates can be met.

    python r3_chroma.py   ->  logs/f1_chroma.txt, f1_chroma.json
"""
import numpy as np

import r3lib as R3
import splatlib as S

RAD = 8
lines = ['ROADS3 gate F1 part 3 -- hue, detail strength, and the law simulated',
         '']
out = {}


def local_ground_rgb(rgb, mask, r=RAD):
    """Per-channel mean of the NON-road texels in a box of radius r."""
    w = (~mask).astype(np.float64)
    den = S._box(w, r)
    g = np.zeros(rgb.shape)
    ok = den * (2 * r + 1) ** 2 >= 20
    for k in range(3):
        num = S._box(rgb[:, :, k] * w, r)
        g[:, :, k] = np.where(ok, num / np.maximum(den, 1e-9), rgb[:, :, k])
    return g, ok


def opp(rgb, m):
    r, g, b = rgb[:, :, 0][m], rgb[:, :, 1][m], rgb[:, :, 2][m]
    return dict(R=float(r.mean()), G=float(g.mean()), B=float(b.mean()),
                b_y=float((b - (r + g) / 2).mean()), r_g=float((r - g).mean()))


for tile in ('t2020', 't0808'):
    cx, cy = R3.TILES[tile]
    A = R3.ours('rung_roads', tile)
    G = R3.ours('rung_noroads', tile)
    D = R3.ours('rung_detail1', tile)
    V = R3.vanilla(tile)
    mask = R3.road_mask(A, G)
    sd = R3.signed_dist(mask)
    sur = (sd <= -1) & (sd >= -8)
    Lv, La, Lg = R3.L(V), R3.L(A), R3.L(G)
    t = {}
    lines += ['=' * 78, 'TILE (%d,%d)   %d road texels' % (cx, cy, mask.sum()), '']

    # ---------------------------------------------------------------- 1 hue
    t['rise_channels'] = {}
    lines += ['  1. ROAD MINUS SURROUND, per channel and per opponent axis',
              '     field         dR      dG      dB     d(b_y)   d(r_g)   dL      dSat']
    for name, rgb in (('vanilla', V), ('ours (rung)', A), ('our ground', G)):
        a_, b_ = opp(rgb, mask), opp(rgb, sur)
        _, s, _ = R3.hsv(rgb)
        row = dict(dR=a_['R'] - b_['R'], dG=a_['G'] - b_['G'], dB=a_['B'] - b_['B'],
                   d_by=a_['b_y'] - b_['b_y'], d_rg=a_['r_g'] - b_['r_g'],
                   dL=float(R3.L(rgb)[mask].mean() - R3.L(rgb)[sur].mean()),
                   dSat=float(s[mask].mean() - s[sur].mean()),
                   road=a_, surround=b_)
        t['rise_channels'][name] = row
        lines.append('     %-12s %+6.2f  %+6.2f  %+6.2f  %+6.2f  %+6.2f  %+6.2f  %+.4f'
                     % (name, row['dR'], row['dG'], row['dB'], row['d_by'],
                        row['d_rg'], row['dL'], row['dSat']))
    lines.append('')

    # ------------------------------------------------- 2 the detail strength
    gl, _ = local_ground_rgb(G, mask)
    # the best WASH: result = ground + (paint - ground) * a, a per texel chosen
    # so the luminance sits `rise` above the LOCAL non-road ground
    rise_van = t['rise_channels']['vanilla']['dL']
    target = R3.L(gl) + rise_van
    den = (La - Lg)
    a_law = np.zeros(den.shape)
    np.divide(target - Lg, den, out=a_law, where=np.abs(den) > 1e-6)
    a_law = np.clip(a_law, 0.0, 1.0)
    W = G + (A - G) * a_law[:, :, None]
    res = Lv - R3.L(W)
    det = R3.L(D) - R3.L(A)
    m = mask
    rows = []
    for r in (0, 1, 2, 4):
        sig = det if r == 0 else S._box(det, r)
        c = float(np.corrcoef(res[m], sig[m])[0, 1])
        k = float((res[m] * sig[m]).sum() / max((sig[m] ** 2).sum(), 1e-9))
        # FLOOR: the same signal with its phase broken, amplitude kept
        fl = []
        for seed in range(1, 6):
            tw = S.phase_twin(sig, seed=seed)
            fl.append(abs(float(np.corrcoef(res[m], tw[m])[0, 1])))
        rows.append(dict(blur=r, corr=c, best_k=k, sig_sd=float(sig[m].std()),
                         floor_mean=float(np.mean(fl)), floor_max=float(np.max(fl))))
    t['detail'] = dict(rows=rows, res_sd=float(res[m].std()))
    lines += ['  2. DOES VANILLA KEEP THE ROAD DIFFUSE`S OWN DETAIL?',
              '     residual after the wash: SD %.2f levels' % res[m].std(),
              '     blur  corr     best strength   signal SD   FLOOR |corr| mean / max']
    for r in rows:
        lines.append('     r=%d   %+.4f   %+7.3f        %5.2f       %.4f / %.4f'
                     % (r['blur'], r['corr'], r['best_k'], r['sig_sd'],
                        r['floor_mean'], r['floor_max']))
    lines.append('')

    # ------------------------------------------- 3 the candidate law simulated
    def gates(sheet, tag):
        Ls = R3.L(sheet)
        sdl = R3.local_sd(Ls)
        pv = R3.profile(Lv, sd)
        ps = R3.profile(Ls, sd)
        step, at = R3.second_difference(ps)
        stepv, atv = R3.second_difference(pv)
        o = opp(sheet, mask)
        ov = opp(V, mask)
        return dict(tag=tag,
                    road_L=float(Ls[mask].mean()), van_L=float(Lv[mask].mean()),
                    dL=float(Ls[mask].mean() - Lv[mask].mean()),
                    rise=float(Ls[mask].mean() - Ls[sur].mean()),
                    van_rise=float(Lv[mask].mean() - Lv[sur].mean()),
                    sd=float(sdl[mask].mean()),
                    van_sd=float(R3.local_sd(Lv)[mask].mean()),
                    d_by=o['b_y'] - ov['b_y'], d_rg=o['r_g'] - ov['r_g'],
                    step=step, step_at=at, van_step=stepv, van_step_at=atv)

    sims = [gates(A, 'rung (opaque paint)'), gates(W, 'wash, rise %.2f' % rise_van)]
    for a in (0.25, 0.5, 0.75):
        sims.append(gates(G + (A - G) * a, 'flat opacity %.2f' % a))
    t['sim'] = sims
    lines += ['  3. THE CANDIDATE LAW, SIMULATED on the rung`s own sheets',
              '     variant               road L   dL vs van   rise   van rise   '
              'SD    van SD   step   van step']
    for s in sims:
        lines.append('     %-20s %6.2f   %+7.2f   %+6.2f   %+6.2f   %5.2f  %5.2f   '
                     '%5.2f   %5.2f'
                     % (s['tag'], s['road_L'], s['dL'], s['rise'], s['van_rise'],
                        s['sd'], s['van_sd'], s['step'], s['van_step']))
    lines += ['     (hue, ours minus vanilla, on the wash: d(b_y) %+.2f d(r_g) %+.2f)'
              % (sims[1]['d_by'], sims[1]['d_rg']), '']
    t['a_law'] = dict(median=float(np.median(a_law[mask])),
                      p10=R3.pct(a_law[mask], 10), p90=R3.pct(a_law[mask], 90),
                      at_edge=float(a_law[mask & (sd == 1)].mean()),
                      at_core=float(a_law[mask & (sd >= 6)].mean())
                      if (mask & (sd >= 6)).sum() else float('nan'))
    lines += ['     the per-texel opacity the wash used: median %.3f  p10 %.3f  '
              'p90 %.3f  edge %.3f  core %.3f'
              % (t['a_law']['median'], t['a_law']['p10'], t['a_law']['p90'],
                 t['a_law']['at_edge'], t['a_law']['at_core']), '']
    out[tile] = t

R3.log(lines, 'f1_chroma.txt')
R3.dump('f1_chroma.json', out)
