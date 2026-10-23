"""Lane SKYA (ATMOCHECK1): the physical sky's model (tests/spells/atmo_model.py, which the GPU LUTs are gated against
by atmo_lut_check.py) cross-checked against an independent port of Bruneton 2017's reference formulation
("Precomputed Atmospheric Scattering: a New Implementation", E. Bruneton, 2017, functions.glsl):
  - transmittance: the optical length to the top by the trapezoidal rule (500 samples,
    ComputeOpticalLengthToTopAtmosphereBoundary), exp(-sum)
  - single scattering: the trapezoidal integral (ComputeSingleScattering; 400 samples here, Bruneton's demo uses 50) of
    T(camera -> point) x T(point -> sun) x density, where T(camera -> point) = T(r, mu) / T(r_d, mu_d)
    (GetTransmittance), Rayleigh phase 3 / (16 pi) (1 + nu^2) (RayleighPhaseFunction)
  - Mie phase: Bruneton's Cornette-Shanks (MiePhaseFunction) vs the model's Henyey-Greenstein (Hillaire 2020)
The medium is the same (Bruneton's demo constants are Hillaire's: Rayleigh 5.802/13.558/33.1e-3 /km, H 8 km; Mie
3.996e-3 scattering, 4.40e-3 extinction, H 1.2 km; ozone tent at 25 km +- 15 km). No haze (Bruneton has none).
Runs no NifSkope, no GPU, a few seconds.

    python -I tests/spells/atmo_bruneton_check.py [--red noozone]

Gates (each with Bruneton's own numerics against the model's):
  transmittance   at 8 zenith angles, relative < 0.002
  rayleigh SS     Rayleigh-only single scattering at 12 view/sun pairs, relative < 0.005
  mie SS          Rayleigh + Mie single scattering with the same (HG) phase, relative < 0.005
Reported, not gated: the Cornette-Shanks vs Henyey-Greenstein difference (the phase choice, a modelling decision),
and Bruneton's own 50-sample error against 400 (up to 11 % in red for a low sun over the 1.2 km Mie layer: his
demo's sample count, not a model difference).
Red noozone: the Bruneton side drops the ozone; the transmittance gate must fail.
"""
import math, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atmo_model as A   # noqa: E402

RB, RT = A.RB, A.RT
RAY = np.array([5.802, 13.558, 33.1]) * 1e-3
MIE_S, MIE_E, G = 3.996e-3, 4.40e-3, 0.8
OZ = np.array([0.650, 1.881, 0.085]) * 1e-3


def density(h, ozone=True):
    h = max(h, 0.0)
    dr, dm = math.exp(-h / 8.0), math.exp(-h / 1.2)
    # Bruneton's ozone: two linear layers, h / 15 - 2 / 3 below 25 km, -h / 15 + 8 / 3 above (the tent)
    do = max(0.0, min(1.0, h / 15.0 - 2.0 / 3.0 if h < 25.0 else -h / 15.0 + 8.0 / 3.0)) if ozone else 0.0
    return dr, dm, do


def dist_top(r, mu):
    disc = r * r * (mu * mu - 1.0) + RT * RT
    return max(0.0, -r * mu + math.sqrt(max(disc, 0.0)))


def transmittance_to_top(r, mu, ozone=True, n=500):
    d = dist_top(r, mu)
    dx = d / n
    od = np.zeros(3)
    for i in range(n + 1):
        di = i * dx
        ri = math.sqrt(di * di + 2.0 * r * mu * di + r * r)
        dr, dm, do = density(ri - RB, ozone)
        w = 0.5 if i in (0, n) else 1.0
        od += w * dx * (RAY * dr + MIE_E * dm + OZ * do)
    return np.exp(-od)


def phase_r(nu):
    return 3.0 / (16.0 * math.pi) * (1.0 + nu * nu)


def phase_cs(nu):   # Bruneton 2017 MiePhaseFunction (Cornette-Shanks)
    k = 3.0 / (8.0 * math.pi) * (1.0 - G * G) / (2.0 + G * G)
    return k * (1.0 + nu * nu) / (1.0 + G * G - 2.0 * G * nu) ** 1.5


def phase_hg(nu):
    return (1.0 - G * G) / (4.0 * math.pi * (1.0 + G * G - 2.0 * G * nu) ** 1.5)


def single_scattering(r, mu, mus, nu, mie=True, mie_phase=phase_hg, ozone=True, n=400):
    """Bruneton's ComputeSingleScattering, the camera ray to the top (views above the horizon)"""
    d = dist_top(r, mu)
    dx = d / n
    t_cam = transmittance_to_top(r, mu, ozone)
    ray, mi = np.zeros(3), np.zeros(3)
    for i in range(n + 1):
        di = i * dx
        rd = math.sqrt(di * di + 2.0 * r * mu * di + r * r)
        mud = (r * mu + di) / rd
        musd = (r * mus + di * nu) / rd
        t_to = t_cam / transmittance_to_top(rd, mud, ozone)   # GetTransmittance (camera -> point)
        t_sun = transmittance_to_top(rd, musd, ozone) if not (musd < 0 and rd * rd * (musd * musd - 1) + RB * RB >= 0) \
            else np.zeros(3)
        dr, dm, _ = density(rd - RB, ozone)
        w = 0.5 if i in (0, n) else 1.0
        ray += w * dx * t_to * t_sun * dr
        mi += w * dx * t_to * t_sun * dm
    L = ray * RAY * phase_r(nu)
    if mie:
        L = L + mi * MIE_S * mie_phase(nu)
    return L


def model_ss(P, r, view, sun, n=400):
    L, _ = A.integrate(P, r, view[None, :], sun, n, psi=None)
    return L[0]


def main():
    red = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else ''
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        print(('PASS ' if ok else 'FAIL ') + name + ': ' + msg)
        fails += 0 if ok else 1

    P = A.params()
    r = RB + 0.002
    worst = 0.0
    for mu in (1.0, 0.8, 0.5, 0.3, 0.15, 0.08, 0.04, 0.02):
        b = transmittance_to_top(r, mu, ozone=red != 'noozone')
        m = A.transmittance(P, np.array([r]), np.array([mu]), 4000)[0]
        worst = max(worst, float(np.abs(b / m - 1).max()))
    gate('transmittance', worst < 0.002, 'Bruneton vs the model, worst relative %.5f (< 0.002) at 8 zenith angles' % worst)

    pairs = []
    for sun_el in (45.0, 10.0, 2.0):
        for (el, az) in ((80.0, 0.0), (20.0, 0.0), (20.0, 90.0), (5.0, 180.0)):
            pairs.append((sun_el, el, az))
    Pr = dict(P)
    Pr['mieS'] = 0.0
    Pr['mieE'] = 0.0
    worst_r = worst_m = worst_cs = worst_50 = 0.0
    for sun_el, el, az in pairs:
        v, s = A.view_dir(el, az), A.sun_dir(sun_el)
        mu, mus, nu = v[2], s[2], float(v @ s)
        # Rayleigh only: the model with no Mie, Bruneton with mie=False and its transmittance without Mie extinction
        global MIE_E
        keep = MIE_E
        MIE_E = 0.0
        b = single_scattering(r, mu, mus, nu, mie=False, ozone=red != 'noozone')
        MIE_E = keep
        m = model_ss(Pr, r, v, s)
        worst_r = max(worst_r, float(np.abs(b / m - 1).max()))
        b = single_scattering(r, mu, mus, nu, mie=True, mie_phase=phase_hg, ozone=red != 'noozone')
        m = model_ss(P, r, v, s)
        worst_m = max(worst_m, float(np.abs(b / m - 1).max()))
        bcs = single_scattering(r, mu, mus, nu, mie=True, mie_phase=phase_cs, ozone=red != 'noozone')
        worst_cs = max(worst_cs, float(np.abs(bcs / b - 1).max()))
        b50 = single_scattering(r, mu, mus, nu, mie=True, mie_phase=phase_hg, ozone=red != 'noozone', n=50)
        worst_50 = max(worst_50, float(np.abs(b50 / b - 1).max()))
    gate('rayleigh SS', worst_r < 0.005, 'Rayleigh-only single scattering, worst relative %.4f (< 0.005) at %d pairs'
         % (worst_r, len(pairs)))
    gate('mie SS', worst_m < 0.005, 'Rayleigh + Mie (HG both), worst relative %.4f (< 0.005)' % worst_m)
    print('INFO samples: Bruneton at 50 samples (his demo) vs 400, worst relative %.3f' % worst_50)
    print('INFO phase: Cornette-Shanks (Bruneton) vs Henyey-Greenstein (Hillaire, ours), worst relative %.3f' % worst_cs)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
