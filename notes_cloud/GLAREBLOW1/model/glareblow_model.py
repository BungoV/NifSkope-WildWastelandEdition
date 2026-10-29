"""Lane GLAREBLOW1 step 1: the cell air's sky term near the sun, offline, against references.

Ours (res/shaders/cell_volair.frag airSun + airMsSum, lookdev_fog.glsl wwVolAirSky): along a sky ray the air scatters
    J(z, c) = D x [ vis Ts pi p(c)  +  sum_{i=1,2} a^i vis_i Ts^(b^i) pi p_{g c^i}(c) ]      (Oz 2013 octaves, a=b=c=1/2)
with D the cell's directional colour, Ts = exp(-sigma0 col(z, top) / mu_sun) the air above the point, and the sky ray's
light L = int sigma0 rho T_view J dt (the froxels, then the closed-form tail to the air's end). Display = pi x radiance.

References:
  MC     backward Monte Carlo of the SAME medium and phase, all orders (delta tracking, next-event to the sun, a
         Lambertian ground of albedo A). Its first order must equal our single scattering (gate); its higher orders
         are the multiple scattering the octaves stand in for.
  PRE    Preetham, Shirley, Smits 1999 (Perez distribution, absolute zenith luminance Yz(T, theta_s) in kcd/m2) with
         the sun's direct normal illuminance from Kasten's Linke formula, ground = A x global / pi.
  CIE    CIE S 011 / ISO 15469 type 12 (clear, low turbidity) relative distribution, scaled to PRE's zenith.
  HIL    tests/spells/atmo_model.py (Hillaire 2020, MC-validated in-repo), clear air, all orders.

    python3 -I glareblow_model.py [--paths N] [--sun ELEV ...]   # prints tables A-C (the fix's columns: table_final.py)
"""
import math, sys, os
import numpy as np

PI = math.pi
LUMK = np.array([0.2126, 0.7152, 0.0722])


# ---------------------------------------------------------------- the phase (src/gl/cellvolair.cpp vaPhase, uncoloured)
FALLBACK_REC = [0.60, 1.52, 3.31, 2, 2, 2, 1, 1, 1, 0.75, 0, 1]   # cellvolair.cpp: the phase with no record


def va_phase(m):
    tot = np.array([m[c] + m[3 + c] + m[6 + c] for c in range(3)])
    inv = 1.0 / float(tot @ LUMK)
    w = [float(np.array(m[k * 3:k * 3 + 3]) @ LUMK) * inv for k in range(3)]
    return dict(w=w, g=[0.0, m[9], m[10]])


def hg(c, g):
    d = 1.0 + g * g - 2.0 * g * c
    return (1.0 - g * g) / (4.0 * PI * d * np.sqrt(d))


def phase(ph, c, gk=1.0):
    """the phase (per sr); gk scales the lobes' g (an octave's c^i)"""
    return (ph['w'][0] * 3.0 / (16.0 * PI) * (1.0 + c * c) + ph['w'][1] * hg(c, ph['g'][1] * gk)
            + ph['w'][2] * hg(c, ph['g'][2] * gk))


def sample_phase(ph, rng, n):
    """cosines drawn from the mixture"""
    u = rng.random(n)
    k = np.where(u < ph['w'][0], 0, np.where(u < ph['w'][0] + ph['w'][1], 1, 2))
    c = np.empty(n)
    # Rayleigh: rejection on (1 + c^2) / 2
    m = k == 0
    cr = rng.random(m.sum()) * 2 - 1
    bad = rng.random(cr.size) * 2 > 1 + cr * cr
    while bad.any():
        cr[bad] = rng.random(bad.sum()) * 2 - 1
        bad[bad] = rng.random(bad.sum()) * 2 > 1 + cr[bad] ** 2
    c[m] = cr
    for kk in (1, 2):
        mk = k == kk
        g = ph['g'][kk]
        x = rng.random(mk.sum())
        if abs(g) < 1e-4:
            c[mk] = 2 * x - 1
        else:
            s = (1 - g * g) / (1 - g + 2 * g * x)
            c[mk] = (1 + g * g - s * s) / (2 * g)
    return np.clip(c, -1, 1)


def rotate(d, c, rng):
    """a direction at cosine c from d (unit rows), uniform azimuth"""
    n = d.shape[0]
    phi = rng.random(n) * 2 * PI
    s = np.sqrt(np.maximum(0, 1 - c * c))
    a = np.where(np.abs(d[:, 2:3]) < 0.9, np.array([[0, 0, 1.0]]), np.array([[1.0, 0, 0]]))
    t = np.cross(d, a)
    t /= np.linalg.norm(t, axis=1)[:, None]
    b = np.cross(d, t)
    return d * c[:, None] + (t * np.cos(phi)[:, None] + b * np.sin(phi)[:, None]) * s[:, None]


# ---------------------------------------------------------------- the medium (cell_airmed.glsl airRho / airCol, one band)
class Medium:
    """sigma0 per km, rho(z) = min(1, exp(-(z - zb) / H)) below top, 0 above (km, the fog's height frame)"""
    def __init__(self, sigma0, zb, H, top):
        self.s0, self.zb, self.H, self.top = sigma0, zb, H, top

    def rho(self, z):
        z = np.asarray(z, float)
        r = np.minimum(1.0, np.exp(-(z - self.zb) / self.H))
        return np.where(z < self.top, r, 0.0)

    def F(self, z):
        """the column from 0 to z per unit sigma0"""
        z = np.minimum(np.asarray(z, float), self.top)
        lo = np.minimum(z, self.zb)
        hi = np.maximum(z - self.zb, 0.0)
        return lo + self.H * (1 - np.exp(-hi / self.H))

    def col(self, a, b):
        return self.F(b) - self.F(a)

    def t_up(self, z, mu):
        return np.exp(-self.s0 * self.col(z, self.top) / np.maximum(mu, 0.02))

    def tau_v(self):
        return self.s0 * float(self.F(self.top))


# ---------------------------------------------------------------- ours: the shader's law along a sky ray
def ours_J(med, ph, z, c, mu_s, ms=True, fix=None):
    """J / D (display: pi x the phase) at heights z for a view-sun cosine c (vis 1: open sky)"""
    Ts = med.t_up(z, mu_s)
    ss = Ts * PI * phase(ph, c)
    if not ms:
        return ss, 0.0 * ss
    a = b = g = 1.0
    msum = 0.0 * ss
    for i in (1, 2):
        b *= 0.5
        g *= 0.5
        if fix is None:
            a *= 0.5
            ai = a
        else:
            ai = fix(z, mu_s, i)
        msum = msum + ai * Ts ** b * PI * phase(ph, c, g)
    return ss, msum


def ours_sky(med, ph, z_eye, elev_v, gamma, mu_s, n=4000, fix=None):
    """the sky ray's light (display, per D): (single, multiple, 1 - T) by exact quadrature to the air's end"""
    s = math.sin(math.radians(elev_v))
    if s > 1e-4:
        dend = max(med.top - z_eye, 0) / s
        tq = dend * (np.arange(n) + 0.5) / n
        dt = dend / n
    else:
        # level: the air never ends; the light saturates at J (1 - T -> 1)
        tq = np.linspace(0, 1, n)
        dt = None
    if dt is None:
        ss, ms = ours_J(med, ph, np.array([z_eye]), math.cos(math.radians(gamma)), mu_s, fix=fix)
        return float(ss[0]), float(ms[0]), 1.0
    z = z_eye + s * tq
    sig = med.s0 * med.rho(z)
    tau = np.cumsum(sig * dt) - sig * dt * 0.5
    Tv = np.exp(-tau)
    c = math.cos(math.radians(gamma))
    ss, ms = ours_J(med, ph, z, c, mu_s, fix=fix)
    w = sig * Tv * dt
    return float((w * ss).sum()), float((w * ms).sum()), float(1 - math.exp(-(sig * dt).sum()))


# ---------------------------------------------------------------- MC: all orders of the same medium (plane-parallel)
def mc_sky(med, ph, z_eye, vdir, sdir, albedo=0.2, paths=40000, seed=7, max_order=60):
    """display light per D (pi x radiance, sun irradiance D at the top of the air): returns (order 1, orders >= 2),
    the ground's bounce counted with the higher orders"""
    rng = np.random.default_rng(seed)
    N = paths
    pos = np.zeros((N, 3))
    pos[:, 2] = z_eye
    d = np.tile(vdir, (N, 1)).astype(float)
    w = np.ones(N)
    alive = np.ones(N, bool)
    L1 = 0.0
    L2 = 0.0
    smax = med.s0
    mu_s = sdir[2]
    order = 0
    while alive.any() and order < max_order:
        idx = np.nonzero(alive)[0]
        p, dd = pos[idx], d[idx]
        # delta tracking to the next real collision, the ground, or out of the top
        t = np.zeros(idx.size)
        state = np.zeros(idx.size, int)  # 0 tracking, 1 collided, 2 ground, 3 escaped
        while (state == 0).any():
            m = state == 0
            t[m] += -np.log(1 - rng.random(m.sum())) / smax
            z = p[m, 2] + dd[m, 2] * t[m]
            st = np.zeros(m.sum(), int)
            st[z <= 0] = 2
            st[(z >= med.top) & (dd[m, 2] > 0)] = 3
            inside = st == 0
            acc = rng.random(m.sum()) < med.rho(np.clip(z, 0, None)) * inside
            st[acc] = 1
            # level rays far away: count as never escaping (they keep scattering), cap at 1e4 km
            st[(st == 0) & (t[m] > 1e4)] = 3
            sub = np.nonzero(m)[0]
            state[sub] = np.where(st == 0, 0, st)
        # ground: t to z = 0
        g = state == 2
        tg = np.where(g, -p[:, 2] / np.where(dd[:, 2] < 0, dd[:, 2], -1), t)
        t = np.where(g, tg, t)
        np_ = p + dd * t[:, None]
        np_[g, 2] = 0.0
        wi = w[idx]
        order += 1
        # collisions: next event to the sun (no shadowing)
        col = state == 1
        if col.any():
            c = dd[col] @ sdir
            Ts = med.t_up(np_[col, 2], mu_s)
            contrib = wi[col] * PI * phase(ph, c) * Ts
            if order == 1:
                L1 += contrib.sum()
            else:
                L2 += contrib.sum()
        if g.any() and albedo > 0:
            Ts = med.t_up(np.zeros(g.sum()), mu_s)
            L2 += (wi[g] * albedo * max(mu_s, 0) * Ts).sum()   # pi x (A / pi) x mu x E
        # continue
        newd = dd.copy()
        nw = wi.copy()
        if col.any():
            newd[col] = rotate(dd[col], sample_phase(ph, rng, col.sum()), rng)
        if g.any():
            u1, u2 = rng.random(g.sum()), rng.random(g.sum())
            r = np.sqrt(u1)
            newd[g] = np.stack([r * np.cos(2 * PI * u2), r * np.sin(2 * PI * u2), np.sqrt(1 - u1)], 1)
            nw[g] *= albedo
        keep = col | (g & (albedo > 0))
        pos[idx] = np_
        d[idx] = newd
        w[idx] = nw
        alive[idx] = keep
        # roulette past order 4
        if order > 4:
            q = rng.random(N) < 0.2
            kill = alive & q
            alive &= ~kill
            w[alive] /= 0.8
    return L1 / N, L2 / N


# ---------------------------------------------------------------- PRE / CIE: absolute clear-sky references
def perez(theta, gamma, A, B, C, D, E):
    return (1 + A * np.exp(B / np.maximum(np.cos(theta), 0.01))) * (1 + C * np.exp(D * gamma) + E * np.cos(gamma) ** 2)


def preetham_Y(T, theta_s, theta, gamma):
    """luminance cd/m2 (Preetham 1999 appendix: Y coefficients and the zenith luminance in kcd/m2)"""
    A, B, C, D, E = 0.1787 * T - 1.4630, -0.3554 * T + 0.4275, -0.0227 * T + 5.3251, 0.1206 * T - 2.5771, -0.0670 * T + 0.3703
    chi = (4.0 / 9.0 - T / 120.0) * (PI - 2 * theta_s)
    Yz = ((4.0453 * T - 4.9710) * math.tan(chi) - 0.2155 * T + 2.4192) * 1000.0
    return Yz * perez(theta, gamma, A, B, C, D, E) / perez(0.0, theta_s, A, B, C, D, E)


def cie_clear(theta, gamma, theta_s, a=-1.0, b=-0.32, c=10.0, d=-3.0, e=0.45):
    """CIE S 011 type 12 (clear, low turbidity): L / Lz"""
    def phi(z):
        return 1 + a * np.exp(b / np.maximum(np.cos(z), 0.01))

    def f(x):
        return 1 + c * (np.exp(d * x) - math.exp(d * PI / 2)) + e * np.cos(x) ** 2
    return f(gamma) * phi(theta) / (f(theta_s) * phi(0.0))


def kasten_direct(elev_deg, TL):
    """direct normal illuminance, lx (Kasten 1980 / Louche 1986 Rayleigh optical thickness, Kasten-Young air mass)"""
    h = math.radians(elev_deg)
    m = 1.0 / (math.sin(h) + 0.50572 * (elev_deg + 6.07995) ** -1.6364)
    dR = 1.0 / (6.6296 + 1.7513 * m - 0.1202 * m ** 2 + 0.0065 * m ** 3 - 0.00013 * m ** 4)
    return 128000.0 * math.exp(-dR * m * TL)


def hemi_diffuse(fn, n=96):
    """the horizontal irradiance of a sky radiance fn(theta, phi) (phi = azimuth from the sun)"""
    th = (np.arange(n) + 0.5) / n * PI / 2
    phs = (np.arange(2 * n) + 0.5) / (2 * n) * 2 * PI
    TH, PH = np.meshgrid(th, phs, indexing='ij')
    v = fn(TH, PH)
    return float((v * np.cos(TH) * np.sin(TH)).sum() * (PI / 2 / n) * (2 * PI / (2 * n)))


def gamma_of(theta, phi, theta_s):
    return np.arccos(np.clip(np.cos(theta) * math.cos(theta_s) + np.sin(theta) * math.sin(theta_s) * np.cos(phi), -1, 1))


def almucantar_phi(elev_deg, gamma_deg):
    """the azimuth from the sun of the point at the sun's elevation, gamma from it (None past the almucantar's reach)"""
    e, g = math.radians(elev_deg), math.radians(gamma_deg)
    c = (math.cos(g) - math.sin(e) ** 2) / max(math.cos(e) ** 2, 1e-9)
    if c < -1:
        return None
    return math.acos(min(1.0, c))


# ---------------------------------------------------------------- main: the tables
GAMMAS = [1, 2, 3, 5, 10, 20, 35, 40, 45, 60, 90]


def hil_ratio(e, gammas, n=40):
    """HIL: Hillaire clear air (tests/spells/atmo_model.py, all orders through its MS LUT): sky / sunlit ground
    (albedo 0.2, the sun + the sky's own light) at the sun's elevation, gamma from the sun"""
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'tests', 'spells'))
    import atmo_model as am
    P = am.params()
    psi = HIL_PSI.get('t')
    if psi is None:
        psi = HIL_PSI['t'] = am.MsTable(P, nu=16, nv=8, k=8, n=16)
    h = 0.01
    sd = am.sun_dir(e)
    # the ground's irradiance: the sun through the air + the sky over the hemisphere
    Tsun = am.transmittance(P, np.array([am.RB + h]), np.array([sd[2]]))[0]
    nt, nph = 12, 24
    th = (np.arange(nt) + 0.5) / nt * PI / 2
    phs = (np.arange(nph) + 0.5) / nph * 2 * PI
    views = [[math.sin(t) * math.cos(f), math.sin(t) * math.sin(f), math.cos(t)] for t in th for f in phs]
    Ls = am.sky(P, h, views, e, psi, n=n)
    wts = np.array([math.cos(t) * math.sin(t) for t in th for f in phs]) * (PI / 2 / nt) * (2 * PI / nph)
    Ed = (Ls * wts[:, None]).sum(0)
    Eg = Tsun * sd[2] + Ed
    Lg = float((0.2 * Eg / PI) @ LUMK)
    out = []
    for gm in gammas:
        phi = almucantar_phi(e, gm)
        v = am.view_dir(e, math.degrees(phi))
        out.append(float(am.sky(P, h, [v], e, psi, n=n)[0] @ LUMK) / Lg)
    return out


HIL_PSI = {}


def main(argv):
    paths = 40000
    suns = [10.0, 20.0, 30.0]
    if '--paths' in argv:
        paths = int(argv[argv.index('--paths') + 1])
    if '--sun' in argv:
        i = argv.index('--sun') + 1
        suns = []
        while i < len(argv) and not argv[i].startswith('--'):
            suns.append(float(argv[i]))
            i += 1
    ph = va_phase(FALLBACK_REC)
    print('phase (fallback record, uncoloured): weights air %.4f fwd %.4f back %.4f, g fwd %.2f back %.2f'
          % (ph['w'][0], ph['w'][1], ph['w'][2], ph['g'][1], ph['g'][2]))
    # media: km. The Concord-like fit (owed live: the 'cell air:' census line's sigma0 zb H top)
    media = {
        'thin':    Medium(0.10, 0.0, 0.5, 3.0),
        'concord': Medium(0.48, 0.0, 0.3, 2.0),
        'thick':   Medium(1.50, 0.0, 0.3, 2.0),
    }
    z_eye = 0.002
    for name, med in media.items():
        print('\nmedium %s: sigma0 %.3f /km H %.2f km top %.1f km, vertical optical depth %.3f'
              % (name, med.s0, med.H, med.top, med.tau_v()))
    print('\n== table A: ours (single | octaves) vs MC of the same medium (single | all higher orders), per D, view at the'
          ' sun\'s elevation (almucantar), ground albedo 0.2')
    print('sun  medium   gamma  ours_ss  ours_ms  ms%   mc_ss    mc_ms   ms%   ours/mc  ss_check')
    rows = []
    for e in suns:
        mu_s = math.sin(math.radians(e))
        sdir = np.array([math.cos(math.radians(e)), 0.0, mu_s])
        for name, med in media.items():
            for gm in GAMMAS:
                phi = almucantar_phi(e, gm)
                if phi is None:
                    continue
                vdir = np.array([math.cos(math.radians(e)) * math.cos(phi), math.cos(math.radians(e)) * math.sin(phi), mu_s])
                gam = math.degrees(math.acos(np.clip(vdir @ sdir, -1, 1)))
                oss, oms, _ = ours_sky(med, ph, z_eye, e, gam, mu_s)
                m1, m2 = mc_sky(med, ph, z_eye, vdir, sdir, paths=paths, seed=int(e * 100 + gm))
                rows.append((e, name, gm, oss, oms, m1, m2))
                print('%4.0f %-8s %5.0f  %7.4f  %7.4f %5.1f  %7.4f  %7.4f %5.1f  %7.3f  %6.3f'
                      % (e, name, gm, oss, oms, 100 * oms / (oss + oms), m1, m2, 100 * m2 / max(m1 + m2, 1e-12),
                         (oss + oms) / max(m1 + m2, 1e-12), oss / max(m1, 1e-12)))
    print('\n== table B: sky / sunlit ground (albedo 0.2, sun + sky light), at the sun\'s elevation, gamma from the sun')
    print('sun  gamma   PRE(T=3)  CIE12    HIL     ours(concord)  ours/PRE  ours/HIL  ours_ss/PRE  ours_ss/HIL')
    for e in suns:
        ts = math.radians(90 - e)
        Edn = kasten_direct(e, 3.0)
        Ed = hemi_diffuse(lambda th, p: preetham_Y(3.0, ts, th, gamma_of(th, p, ts)))
        Eg = Edn * math.sin(math.radians(e)) + Ed
        Lg = 0.2 * Eg / PI
        Lz = preetham_Y(3.0, ts, 0.0, ts)
        med = media['concord']
        mu_s = math.sin(math.radians(e))
        # ours: the ground shows A x (D Tup mu + the ambient); the ambient taken at PRE's diffuse share
        k_amb = Ed / (Edn * math.sin(math.radians(e)))
        Lg_o = 0.2 * float(med.t_up(0.0, mu_s)) * mu_s * (1 + k_amb)
        gs = [gm for gm in GAMMAS if almucantar_phi(e, gm) is not None]
        hil = dict(zip(gs, hil_ratio(e, gs)))
        for gm in gs:
            Lp = preetham_Y(3.0, ts, ts, gm * PI / 180)
            Lc = Lz * cie_clear(ts, gm * PI / 180, ts)
            oss, oms, _ = ours_sky(med, ph, z_eye, e, gm, mu_s)
            o = (oss + oms) / Lg_o
            print('%4.0f %5.0f   %7.3f  %7.3f  %7.3f   %7.3f       %6.2f    %6.2f     %6.2f       %6.2f'
                  % (e, gm, Lp / Lg, Lc / Lg, hil[gm], o, o / (Lp / Lg), o / hil[gm], oss / Lg_o / (Lp / Lg),
                     oss / Lg_o / hil[gm]))
    print('\n== table C: the phase\'s forward g (concord medium, view at the sun\'s elevation): ours\' octave share and'
          ' its excess over MC\'s higher orders')
    print('sun  g_fwd  gamma  ours_ms%  mc_ms%  ours/mc')
    for e in suns:
        mu_s = math.sin(math.radians(e))
        sdir = np.array([math.cos(math.radians(e)), 0.0, mu_s])
        for gf in (0.75, 0.5, 0.3):
            rec = list(FALLBACK_REC)
            rec[9] = gf
            php = va_phase(rec)
            for gm in (2, 5, 40):
                phi = almucantar_phi(e, gm)
                vdir = np.array([math.cos(math.radians(e)) * math.cos(phi), math.cos(math.radians(e)) * math.sin(phi), mu_s])
                oss, oms, _ = ours_sky(media['concord'], php, z_eye, e, gm, mu_s)
                m1, m2 = mc_sky(media['concord'], php, z_eye, vdir, sdir, paths=paths, seed=int(e * 7 + gm + gf * 100))
                print('%4.0f  %4.2f  %5.0f   %6.1f   %6.1f   %6.3f'
                      % (e, gf, gm, 100 * oms / (oss + oms), 100 * m2 / (m1 + m2), (oss + oms) / (m1 + m2)))
    return rows


if __name__ == '__main__':
    main(sys.argv[1:])
