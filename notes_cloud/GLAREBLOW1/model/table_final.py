"""GLAREBLOW1: the RESULTS table. Concord-like medium, view at the sun's elevation (almucantar), gamma from the sun.
ours before (a = 1/2) and after (a = min(1/2, f)) against MC of the same medium (all orders), and the sky/ground
ratios of the absolute references (PRE, HIL) beside ours."""
import sys, math, numpy as np
sys.path.insert(0, '.')
import glareblow_model as gm
from proto_fix import make
paths = int(sys.argv[1]) if len(sys.argv) > 1 else 100000
med = gm.Medium(0.48, 0.0, 0.3, 2.0)
fix = make('min', med)
G = [2, 5, 10, 20, 35, 40, 45, 60, 90]
for e in (10, 15, 20):
    mu = math.sin(math.radians(e)); sd = np.array([math.cos(math.radians(e)), 0, mu])
    ts = math.radians(90 - e)
    Edn = gm.kasten_direct(e, 3.0)
    Ed = gm.hemi_diffuse(lambda th, p: gm.preetham_Y(3.0, ts, th, gm.gamma_of(th, p, ts)))
    Lg = 0.2 * (Edn * mu + Ed) / math.pi
    Lg_o = 0.2 * float(med.t_up(0.0, mu)) * mu * (1 + Ed / (Edn * mu))
    hil = dict(zip(G, gm.hil_ratio(e, G)))
    for gf in (0.75, 0.3):
        rec = list(gm.FALLBACK_REC); rec[9] = gf; ph = gm.va_phase(rec)
        print('\nsun %d deg, fwd g %.2f   (sky / sunlit ground, albedo 0.2; ours per the Concord-like medium)' % (e, gf))
        print('gamma | ours SS  MS%(old) MS%(new) MC MS% | old/MC new/MC | PRE   HIL   ours-old ours-new | new/PRE new/HIL')
        for g in G:
            phi = gm.almucantar_phi(e, g)
            v = np.array([math.cos(math.radians(e)) * math.cos(phi), math.cos(math.radians(e)) * math.sin(phi), mu])
            m1, m2 = gm.mc_sky(med, ph, 0.002, v, sd, paths=paths, seed=e * 77 + g + int(gf * 10))
            ss, mo, _ = gm.ours_sky(med, ph, 0.002, e, g, mu)
            _, mn, _ = gm.ours_sky(med, ph, 0.002, e, g, mu, fix=fix)
            pre = gm.preetham_Y(3.0, ts, ts, g * math.pi / 180) / Lg
            print('%5d | %6.3f  %5.1f   %5.1f   %5.1f  | %5.2f  %5.2f  | %5.2f %5.2f  %6.2f  %6.2f  | %5.2f  %5.2f'
                  % (g, ss, 100 * mo / (ss + mo), 100 * mn / (ss + mn), 100 * m2 / (m1 + m2), (ss + mo) / (m1 + m2),
                     (ss + mn) / (m1 + m2), pre, hil[g], (ss + mo) / Lg_o, (ss + mn) / Lg_o, (ss + mn) / Lg_o / pre,
                     (ss + mn) / Lg_o / hil[g]))
