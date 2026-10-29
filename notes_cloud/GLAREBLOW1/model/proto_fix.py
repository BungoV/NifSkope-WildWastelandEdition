"""GLAREBLOW1: candidate octave weights vs MC of the same medium (prototype; the chosen one goes to the shader)"""
import sys, math, numpy as np
sys.path.insert(0, '.')
import glareblow_model as gm

GL4 = (np.array([0.0694318442, 0.3300094782, 0.6699905218, 0.9305681558]),
       np.array([0.1739274226, 0.3260725774, 0.3260725774, 0.1739274226]))

def esc(tau):
    """E2(tau) = int_0^1 exp(-tau / mu) dmu, 4-point Gauss-Legendre"""
    x, w = GL4
    tau = np.asarray(tau, float)
    return sum(wi * np.exp(-tau / xi) for xi, wi in zip(x, w))

def f_rescatter(med, z, ground=0.0):
    tu = med.s0 * med.col(z, med.top)
    td = med.s0 * med.col(0.0, z)
    eu, ed = esc(tu), esc(td)
    return 0.5 * (1 - eu) + 0.5 * (1 - ed) + 0.5 * ed * ground

def make(kind, med):
    if kind == 'f':
        return lambda z, mu, i: f_rescatter(med, z) ** i
    if kind == 'fg':
        return lambda z, mu, i: f_rescatter(med, z, 0.2) ** i
    if kind == 'min':
        return lambda z, mu, i: np.minimum(0.5, f_rescatter(med, z, 0.2)) ** i

def main():
    media = {'thin': gm.Medium(0.10, 0.0, 0.5, 3.0), 'concord': gm.Medium(0.48, 0.0, 0.3, 2.0),
             'thick': gm.Medium(1.50, 0.0, 0.3, 2.0), 'fog': gm.Medium(6.0, 0.0, 0.3, 2.0)}
    kinds = sys.argv[1:] or ['f', 'fg', 'min']
    for name, med in media.items():
        print(name, 'tau_v %.3f f(eye) %.3f fg(eye) %.3f' % (med.tau_v(), f_rescatter(med, 0.002), f_rescatter(med, 0.002, 0.2)))
    print('medium   sun  g    gamma  old/mc  ' + '  '.join('%s/mc' % k for k in kinds))
    worst = {k: 0 for k in ['old'] + kinds}
    for name, med in media.items():
        for e in (5, 15, 30):
            mu = math.sin(math.radians(e)); sd = np.array([math.cos(math.radians(e)), 0, mu])
            for gf in (0.75, 0.3):
                rec = list(gm.FALLBACK_REC); rec[9] = gf; ph = gm.va_phase(rec)
                for g in (2, 40, 90):
                    phi = gm.almucantar_phi(e, g)
                    v = np.array([math.cos(math.radians(e)) * math.cos(phi), math.cos(math.radians(e)) * math.sin(phi), mu])
                    m1, m2 = gm.mc_sky(med, ph, 0.002, v, sd, paths=40000, seed=e * 31 + g + int(gf * 10))
                    ref = m1 + m2
                    o = sum(gm.ours_sky(med, ph, 0.002, e, g, mu)[:2]) / ref
                    cs = [sum(gm.ours_sky(med, ph, 0.002, e, g, mu, fix=make(k, med))[:2]) / ref for k in kinds]
                    worst['old'] = max(worst['old'], abs(math.log(o)))
                    for k, c in zip(kinds, cs):
                        worst[k] = max(worst[k], abs(math.log(c)))
                    print('%-8s %3d %4.2f %5d  %6.3f  ' % (name, e, gf, g, o) + '  '.join('%6.3f' % c for c in cs))
    print('worst |ratio - 1| (log):', {k: round(math.exp(v) - 1, 3) for k, v in worst.items()})


if __name__ == '__main__':
    main()
