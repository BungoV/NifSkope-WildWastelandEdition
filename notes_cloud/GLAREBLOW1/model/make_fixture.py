"""GLAREBLOW1: the Monte Carlo fixture tests/glareblow1_test.cpp reads (tests/fixtures/glareblow1_mc.tsv).
    python3 -I make_fixture.py [paths] > ../../../tests/fixtures/glareblow1_mc.tsv"""
import sys, math, numpy as np
sys.path.insert(0, '.')
import glareblow_model as gm

paths = int(sys.argv[1]) if len(sys.argv) > 1 else 100000
media = [('thin', gm.Medium(0.10, 0.0, 0.5, 3.0)), ('concord', gm.Medium(0.48, 0.0, 0.3, 2.0)),
         ('thick', gm.Medium(1.50, 0.0, 0.3, 2.0))]
print('# lane GLAREBLOW1: backward Monte Carlo (all orders, ground albedo 0.2, sun irradiance 1 at the air top) of the')
print('# cell air medium rho = min(1, exp(-(z - zb) / H)) below top (km) with the fallback god-ray phase, fwd g varied.')
print('# eye 2 m; view at the sun\'s elevation, gamma from the sun. Display units (pi x radiance). paths=%d' % paths)
print('# made by notes_cloud/GLAREBLOW1/model/make_fixture.py')
print('medium\tsigma0\tzb\tH\ttop\tsun\tgfwd\tgamma\tmc_ss\tmc_ms')
for name, med in media:
    for e in (5, 10, 15, 20, 30):
        mu = math.sin(math.radians(e))
        sd = np.array([math.cos(math.radians(e)), 0, mu])
        for gf in (0.75, 0.5, 0.3):
            rec = list(gm.FALLBACK_REC); rec[9] = gf
            ph = gm.va_phase(rec)
            for g in (2, 5, 20, 40, 90):
                phi = gm.almucantar_phi(e, g)
                v = np.array([math.cos(math.radians(e)) * math.cos(phi), math.cos(math.radians(e)) * math.sin(phi), mu])
                m1, m2 = gm.mc_sky(med, ph, 0.002, v, sd, paths=paths, seed=e * 1000 + g * 10 + int(gf * 100))
                print('%s\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%.6f\t%.6f' % (name, med.s0, med.zb, med.H, med.top, e, gf, g, m1, m2))
                sys.stdout.flush()
