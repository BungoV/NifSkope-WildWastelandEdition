"""ROADS3 gate F1 -- vanilla's road law, fitted, with its floor and its ceiling.

Run AFTER `r3_selftest.py` is 14/14.  Reads only shipped vanilla sheets and the
rung exe's own bakes; changes nothing.

    python r3_fit.py   ->  logs/f1_fit.txt, f1_fit.json
"""
import numpy as np

import r3lib as R3
import splatlib as S

lines = ['ROADS3 gate F1 -- vanilla`s road law on the shipped sheets', '']
out = {}

for tile in ('t2020', 't0808'):
    cx, cy = R3.TILES[tile]
    A = R3.ours('rung_roads', tile)          # ours WITH roads, shipped defaults
    G = R3.ours('rung_noroads', tile)        # ours, the ground alone
    D = R3.ours('rung_detail1', tile)        # ours with the diffuse sampled full
    V = R3.vanilla(tile)                     # vanilla's shipped sheet
    mask = R3.road_mask(A, G)
    sd = R3.signed_dist(mask)
    t = {}
    lines += ['=' * 78,
              'TILE (%d,%d)   road mask %d of %d texels (%.2f %%)'
              % (cx, cy, mask.sum(), mask.size, 100.0 * mask.sum() / mask.size),
              '']

    # ---- 0. the assumption that lets the sheet stand in for the road plane
    # at alpha 1 the sheet IS the paint, so detail 0 and detail 1 must differ
    # wherever the material's own texture is not flat
    dd = np.abs(A - D).max(2)
    t['detail_moves'] = dict(n=int(mask.sum()),
                             moved=int((dd[mask] >= 1.0).sum()),
                             mean=float(dd[mask].mean()),
                             p90=R3.pct(dd[mask], 90))
    lines += ['  the road sheet IS the paint (control): detail 0 vs detail 1 differs',
              '    on %d of %d mask texels, mean %.2f levels, p90 %.2f'
              % (t['detail_moves']['moved'], t['detail_moves']['n'],
                 t['detail_moves']['mean'], t['detail_moves']['p90']), '']

    # ---- 1. alignment control: is vanilla's road where ours is?
    best = None
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            Vs = np.roll(np.roll(V, dy, 0), dx, 1)
            f = R3.fit_alpha(Vs, A, G, mask)
            fr = float(f['frac'][f['read']].mean())
            if best is None or fr < best[0]:
                best = (fr, dx, dy)
    t['align'] = dict(best_frac=best[0], dx=best[1], dy=best[2])
    f0 = R3.fit_alpha(V, A, G, mask)
    t['align']['frac_at_0_0'] = float(f0['frac'][f0['read']].mean())
    lines += ['  alignment control: best shift (%d,%d) frac %.4f; at (0,0) %.4f'
              % (best[1], best[2], best[0], t['align']['frac_at_0_0']),
              '    -> the fit is read at (0,0); a better shift elsewhere would '
              'mean the mask does not name vanilla`s road', '']

    # ---- 2. the fit itself
    f = f0
    t['fit'] = R3.summarise(f['a'], f['read'], 'a at (0,0)')
    t['fit']['dropped_small_denom'] = int(mask.sum() - f['read'].sum())
    t['fit']['resid_mean'] = float(f['resid'][f['read']].mean())
    t['fit']['reach_mean'] = float(f['reach'][f['read']].mean())
    t['fit']['frac_mean'] = float(f['frac'][f['read']].mean())
    lines += ['  OPACITY a:  median %.3f  mean %.3f  p10 %.3f  p90 %.3f  sd %.3f'
              % (t['fit']['median'], t['fit']['mean'], t['fit']['p10'],
                 t['fit']['p90'], t['fit']['sd']),
              '    read on %d texels; %d dropped for |R-G| < %.0f levels'
              % (t['fit']['n'], t['fit']['dropped_small_denom'], R3.DENOM_MIN),
              '    residual %.2f levels against a reach of %.2f  (%.1f %% unexplained)'
              % (t['fit']['resid_mean'], t['fit']['reach_mean'],
                 100 * t['fit']['frac_mean']), '']

    # ---- 3. the FLOOR: the same fit with the ground shuffled among road texels
    rng = np.random.default_rng(11)
    fr_sh, med_sh = [], []
    for s in range(5):
        Gs = G.copy()
        idx = np.flatnonzero(mask.ravel())
        flat = Gs.reshape(-1, 3)
        flat[idx] = flat[rng.permutation(idx)]
        fs = R3.fit_alpha(V, A, Gs, mask)
        fr_sh.append(float(fs['frac'][fs['read']].mean()))
        med_sh.append(float(np.median(fs['a'][fs['read']])))
    t['floor_shuffled'] = dict(frac_mean=float(np.mean(fr_sh)),
                               frac_sd=float(np.std(fr_sh)),
                               a_median=float(np.mean(med_sh)))
    lines += ['  FLOOR (ground shuffled among the road texels, 5 draws):',
              '    unexplained %.1f %% (sd %.1f) against the real fit`s %.1f %%'
              % (100 * t['floor_shuffled']['frac_mean'],
                 100 * t['floor_shuffled']['frac_sd'],
                 100 * t['fit']['frac_mean']),
              '    a median %.3f' % t['floor_shuffled']['a_median'], '']

    # ---- 4. the CEILING: vanilla against a neighbouring shipped tile
    try:
        Vn = R3.vanilla(R3.NEIGHBOUR[tile])
        fn = R3.fit_alpha(Vn, A, G, mask)
        t['ceiling_neighbour'] = dict(
            cell=list(R3.NEIGHBOUR[tile]),
            frac_mean=float(fn['frac'][fn['read']].mean()),
            a_median=float(np.median(fn['a'][fn['read']])))
        lines += ['  CEILING (vanilla`s NEIGHBOUR sheet %s under the same mask):'
                  % (R3.NEIGHBOUR[tile],),
                  '    unexplained %.1f %%, a median %.3f'
                  % (100 * t['ceiling_neighbour']['frac_mean'],
                     t['ceiling_neighbour']['a_median']), '']
    except Exception as exc:                                   # noqa: BLE001
        t['ceiling_neighbour'] = dict(error=str(exc))
        lines += ['  CEILING refused: %s' % exc, '']

    # ---- 5. a across the road width
    prof_a = {}
    for d in range(1, 15):
        m = f['read'] & (sd == d)
        if m.sum() >= 20:
            prof_a[d] = (float(f['a'][m].mean()), int(m.sum()))
    t['a_profile'] = {str(k): v for k, v in prof_a.items()}
    lines += ['  a ACROSS THE ROAD (mean a at n texels from the edge):']
    for d in sorted(prof_a):
        lines.append('    d=%-3d  a %.3f   (%d texels)'
                     % (d, prof_a[d][0], prof_a[d][1]))
    lines.append('')

    # ---- 6. a by material.  Under `--road-detail 0` each material paints ONE
    # flat colour, so the distinct colours in the road sheet ENUMERATE the
    # materials (times the mesh's own vertex tint).
    key = (A[:, :, 0].astype(int) * 65536 + A[:, :, 1].astype(int) * 256
           + A[:, :, 2].astype(int))
    vals, counts = np.unique(key[f['read']], return_counts=True)
    order = np.argsort(-counts)
    rows = []
    for i in order[:10]:
        m = f['read'] & (key == vals[i])
        rgb = (int(vals[i]) >> 16, (int(vals[i]) >> 8) & 255, int(vals[i]) & 255)
        rows.append(dict(rgb=rgb, n=int(counts[i]),
                         a_median=float(np.median(f['a'][m])),
                         van_L=float(R3.L(V)[m].mean()),
                         our_L=float(R3.L(A)[m].mean())))
    t['a_by_colour'] = rows
    t['distinct_road_colours'] = int(vals.size)
    lines += ['  a BY PAINT COLOUR (%d distinct flat colours = the materials):'
              % vals.size]
    for r in rows:
        lines.append('    rgb(%3d,%3d,%3d)  %6d texels  a %.3f   ours L %.1f  '
                     'vanilla L %.1f'
                     % (r['rgb'][0], r['rgb'][1], r['rgb'][2], r['n'],
                        r['a_median'], r['our_L'], r['van_L']))
    lines.append('')

    # ---- 7. the residual after a CONSTANT a: does vanilla keep texture detail?
    a_med = t['fit']['median']
    pred = G + (A - G) * a_med
    res = R3.L(V) - R3.L(pred)
    det = R3.L(D) - R3.L(A)        # the diffuse's departure from its own average
    m = f['read']
    rows = []
    for r in (0, 1, 2, 4):
        dsig = det if r == 0 else S._box(det, r)
        c = np.corrcoef(res[m], dsig[m])[0, 1]
        # the best least-squares strength for this signal
        k = float((res[m] * dsig[m]).sum() / max((dsig[m] ** 2).sum(), 1e-9))
        rows.append(dict(blur_radius=r, corr=float(c), best_k=k,
                         sig_sd=float(dsig[m].std())))
    # floor for the correlation: the same signal, rows reversed (a phase break
    # that keeps the amplitude), 5 draws
    rng = np.random.default_rng(5)
    fl = []
    for s in range(5):
        sh = np.roll(np.roll(det, int(rng.integers(40, 470)), 0),
                     int(rng.integers(40, 470)), 1)
        fl.append(float(np.corrcoef(res[m], sh[m])[0, 1]))
    t['detail_residual'] = dict(a_used=a_med, rows=rows,
                                res_sd=float(res[m].std()),
                                floor_corr_mean=float(np.mean(np.abs(fl))),
                                floor_corr_max=float(np.max(np.abs(fl))))
    lines += ['  RESIDUAL after a constant a = %.3f:  SD %.2f levels' % (a_med, res[m].std()),
              '    correlation with the road diffuse`s own detail:']
    for r in rows:
        lines.append('      blur r=%d  corr %+.4f  best strength %+.3f  (signal SD %.2f)'
                     % (r['blur_radius'], r['corr'], r['best_k'], r['sig_sd']))
    lines.append('      FLOOR (same signal, translated; 5 draws): |corr| mean %.4f max %.4f'
                 % (t['detail_residual']['floor_corr_mean'],
                    t['detail_residual']['floor_corr_max']))
    # and with the ground, which is the other candidate
    cg = float(np.corrcoef(res[m], R3.L(G)[m])[0, 1])
    t['detail_residual']['corr_with_ground'] = cg
    lines += ['      correlation with our own GROUND luminance: %+.4f' % cg, '']

    # ---- 8. the hue, named by a measurement
    t['hue'] = dict(vanilla=R3.hue_numbers(V, m), ours=R3.hue_numbers(A, m),
                    ground=R3.hue_numbers(G, m),
                    predicted=R3.hue_numbers(pred, m))
    lines += ['  HUE on the road texels (mean levels; b_y = B-(R+G)/2, r_g = R-G):']
    for k in ('vanilla', 'ours', 'ground', 'predicted'):
        h = t['hue'][k]
        lines.append('    %-10s R %6.2f G %6.2f B %6.2f   b_y %+6.2f  r_g %+6.2f  '
                     'sat %.3f' % (k, h['R'], h['G'], h['B'], h['b_y'],
                                   h['r_g'], h['sat']))
    lines.append('')

    # ---- 9. luminance and SD, the F3 rows
    Lv, La, Lg = R3.L(V), R3.L(A), R3.L(G)
    sdv, sda = R3.local_sd(Lv), R3.local_sd(La)
    t['levels'] = dict(van_L=float(Lv[m].mean()), our_L=float(La[m].mean()),
                       ground_L=float(Lg[m].mean()),
                       pred_L=float(R3.L(pred)[m].mean()),
                       van_sd=float(sdv[m].mean()), our_sd=float(sda[m].mean()))
    lines += ['  LEVELS on the road: vanilla %.2f  ours %.2f  our ground %.2f  '
              'law(a=%.3f) %.2f' % (t['levels']['van_L'], t['levels']['our_L'],
                                    t['levels']['ground_L'], a_med,
                                    t['levels']['pred_L']),
              '  LOCAL 5x5 SD on the road: vanilla %.2f  ours %.2f'
              % (t['levels']['van_sd'], t['levels']['our_sd']), '']

    # ---- 10. the cross-road luminance profile, vanilla vs ours
    t['profile_van'] = {str(k): v for k, v in R3.profile(Lv, sd).items()}
    t['profile_our'] = {str(k): v for k, v in R3.profile(La, sd).items()}
    wv, atv = R3.second_difference(R3.profile(Lv, sd))
    wo, ato = R3.second_difference(R3.profile(La, sd))
    t['step'] = dict(vanilla=wv, vanilla_at=atv, ours=wo, ours_at=ato)
    lines += ['  CROSS-ROAD PROFILE (mean luminance at signed distance):',
              '    d      vanilla    ours    ground   n']
    pv, po = R3.profile(Lv, sd), R3.profile(La, sd)
    pg = R3.profile(Lg, sd)
    for d in sorted(set(pv) | set(po)):
        lines.append('    %+4d   %7.2f  %7.2f  %7.2f   %d'
                     % (d, pv.get(d, (float('nan'), 0))[0],
                        po.get(d, (float('nan'), 0))[0],
                        pg.get(d, (float('nan'), 0))[0],
                        pv.get(d, (0, 0))[1]))
    lines += ['    biggest STEP inside the road (max 2nd difference): '
              'vanilla %.3f at d=%s, ours %.3f at d=%s' % (wv, atv, wo, ato), '']

    out[tile] = t

R3.log(lines, 'f1_fit.txt')
R3.dump('f1_fit.json', out)
