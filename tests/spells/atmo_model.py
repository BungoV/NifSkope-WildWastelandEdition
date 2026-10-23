"""Lane ATMO1: the physical sky's offline model (Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere
Rendering Technique", EGSR). The same math as res/shaders/atmo_*.glsl, in numpy, plus an independent Monte Carlo
path tracer of the same medium (the multiple-scattering reference). Units: km, sun irradiance 1 at the top.

    python -I tests/spells/atmo_model.py [--paths N]     # the gates (prints PASS/FAIL lines, exit 0 only when all pass)
    python -I tests/spells/atmo_model.py --red noozone|noms|nohaze   # each red must fail its gate

Earth constants are Hillaire 2020's table (also Bruneton 2017's demo); see notes/atmores1/RESEARCH.md sec. 2.1.
"""
import math, sys
import numpy as np

RB, RT = 6360.0, 6460.0
UNIT_M = 0.0142875          # one game unit in meters (FO4: 70 units = 1 m)
KOSCH = 3.912               # Koschmieder: -ln(0.02)


def params(haze=(0.0, 1.0, 0.0), ozone=True, ms=True):
    """haze = (ground extinction /km, scale height km, plateau top km): the weather fog's layer (haze_from_fog)"""
    return dict(ray=np.array([5.802, 13.558, 33.1]) * 1e-3, rayH=8.0,
                mieS=3.996e-3, mieE=4.40e-3, mieH=1.2, g=0.8, hazeE=haze[0], hazeH=haze[1], hazeMid=haze[2],
                oz=(np.array([0.650, 1.881, 0.085]) * 1e-3) if ozone else np.zeros(3), ozC=25.0, ozW=15.0,
                albedo=np.array([0.3, 0.3, 0.3]), ms=ms)


def haze_from_fog(far_units, maxv, mid_units=0.0, range_units=10000.0):
    """The haze rung: the record's fog reaches `maxv` at Day/Night-Far; a fog that reaches the 2 % contrast threshold
    there is Koschmieder's visibility (sigma = 3.912 / V). The general form keeps the record's own Max:
    sigma = -ln(1 - min(maxv, 0.98)) / Far. The clear air's ground extinction (Rayleigh + Hillaire's Mie at 550 nm)
    comes off; the rest is a haze layer: flat up to the fog's Far Height Mid, then e-folding over its Range."""
    if far_units <= 0 or maxv <= 0:
        return (0.0, 1.0, 0.0)
    v_km = far_units * UNIT_M / 1000.0
    sigma = -math.log(1.0 - min(maxv, 0.98)) / v_km
    e = max(0.0, sigma - 13.558e-3 - 4.40e-3)
    return (min(e, 50.0), max(0.02, range_units * UNIT_M / 1000.0), max(0.0, mid_units * UNIT_M / 1000.0))


def medium(P, h):
    h = np.asarray(h, dtype=np.float64)
    dr = np.exp(-np.maximum(h, 0) / P['rayH'])
    dm = np.exp(-np.maximum(h, 0) / P['mieH'])
    do = np.maximum(0.0, 1.0 - np.abs(h - P['ozC']) / P['ozW'])
    dh = np.exp(-np.maximum(h - P['hazeMid'], 0) / P['hazeH'])
    rayS = P['ray'] * dr[..., None]
    mieS = (P['mieS'] * dm + P['hazeE'] * (3.996 / 4.40) * dh)[..., None] * np.ones(3)
    ext = rayS + (P['mieE'] * dm + P['hazeE'] * dh)[..., None] + P['oz'] * do[..., None]
    return rayS, mieS, ext


def dist_top(r, mu):
    return np.maximum(0.0, -r * mu + np.sqrt(np.maximum(0.0, r * r * (mu * mu - 1.0) + RT * RT)))


def hits_ground(r, mu):
    return (mu < 0) & (r * r * (mu * mu - 1.0) + RB * RB >= 0)


def dist_ground(r, mu):
    return np.maximum(0.0, -r * mu - np.sqrt(np.maximum(0.0, r * r * (mu * mu - 1.0) + RB * RB)))


def transmittance(P, r, mu, n=64):
    """exact-ish: midpoint integration to the top; 0 when the ray hits the ground"""
    r = np.asarray(r, np.float64)
    mu = np.asarray(mu, np.float64)
    d = dist_top(r, mu)
    t = (np.arange(n) + 0.5) / n
    tt = d[..., None] * t
    rr = np.sqrt(r[..., None] ** 2 + tt ** 2 + 2 * r[..., None] * mu[..., None] * tt)
    _, _, ext = medium(P, rr - RB)
    od = ext.sum(axis=-2) * (d / n)[..., None]
    T = np.exp(-od)
    T[hits_ground(r, mu)] = 0.0
    return T


# ---- the transmittance LUT's parameterization (Bruneton 2017), the same in atmo_common.glsl
def trans_uv_to_rmu(u, v):
    H = math.sqrt(RT * RT - RB * RB)
    rho = H * v
    r = np.sqrt(rho * rho + RB * RB)
    dmin, dmax = RT - r, rho + H
    d = dmin + u * (dmax - dmin)
    mu = np.where(d == 0, 1.0, (H * H - rho * rho - d * d) / np.maximum(2 * r * d, 1e-9))
    return r, np.clip(mu, -1, 1)


def phase_r(c):
    return 3.0 / (16 * math.pi) * (1 + c * c)


def phase_hg(g, c):
    return (1 - g * g) / (4 * math.pi * np.power(1 + g * g - 2 * g * c, 1.5))


def integrate(P, r0, vdir, sdir, n, psi=None, iso=False, ground=False, dmax=None, want_T=False):
    """Hillaire's IntegrateScatteredLuminance for many rays at once. r0 scalar (camera on +z), vdir (N,3), sdir (3,)
    or (N,3). Returns L (N,3) and, for the MS pass, the fms integrand (N,3). Lane SKYA (aerial perspective): dmax
    stops the march there (no ground term); want_T also returns the transmittance over the march."""
    N = vdir.shape[0]
    sdir = np.broadcast_to(sdir, (N, 3))
    mu = vdir[:, 2]
    r0a = np.full(N, r0)
    gnd = hits_ground(r0a, mu)
    d = np.where(gnd, dist_ground(r0a, mu), dist_top(r0a, mu))
    if dmax is not None:
        d = np.minimum(dist_top(r0a, mu), dmax)   # through the ground as the GPU froxel does (the medium clamps h)
        gnd = np.zeros(N, bool)
    L = np.zeros((N, 3))
    F = np.zeros((N, 3))
    thr = np.ones((N, 3))
    cos_vs = (vdir * sdir).sum(1)
    pr, pm = phase_r(cos_vs), phase_hg(P['g'], cos_vs)
    dt = d / n
    for i in range(n):
        t = (i + 0.5) * dt
        p = np.stack([vdir[:, 0] * t, vdir[:, 1] * t, r0 + vdir[:, 2] * t], 1)
        rp = np.linalg.norm(p, axis=1)
        up = p / rp[:, None]
        mus = (up * sdir).sum(1)
        rayS, mieS, ext = medium(P, rp - RB)
        Ts = transmittance(P, rp, mus)
        scat = rayS + mieS
        if iso:
            S = Ts * scat / (4 * math.pi)
        else:
            S = Ts * (rayS * pr[:, None] + mieS * pm[:, None])
        if psi is not None and P['ms']:
            S = S + psi(rp, mus) * scat
        Tst = np.exp(-ext * dt[:, None])
        L += thr * (S - S * Tst) / ext
        F += thr * (scat - scat * Tst) / ext
        thr *= Tst
    if ground:
        pg = np.stack([vdir[:, 0] * d, vdir[:, 1] * d, r0 + vdir[:, 2] * d], 1)
        up = pg / np.linalg.norm(pg, axis=1)[:, None]
        mus = (up * sdir).sum(1)
        Tg = transmittance(P, np.full(N, RB + 1e-4), mus)
        add = thr * Tg * np.maximum(mus, 0)[:, None] * P['albedo'] / math.pi
        L += np.where(gnd[:, None], add, 0)
    if want_T:
        return L, F, thr
    return L, F


def sphere_dirs(k):
    i, j = np.meshgrid((np.arange(k) + 0.5) / k, (np.arange(k) + 0.5) / k)
    th = 2 * math.pi * i.ravel()
    ph = np.arccos(1 - 2 * j.ravel())
    return np.stack([np.sin(ph) * np.cos(th), np.sin(ph) * np.sin(th), np.cos(ph)], 1)


def ms_psi(P, r, mus, k=8, n=20):
    """Hillaire's multiple-scattering LUT texel: Psi = L2 / (1 - f_ms)"""
    dirs = sphere_dirs(k)
    s = np.array([math.sqrt(max(0, 1 - mus * mus)), 0.0, mus])
    L, F = integrate(P, r, dirs, s, n, iso=True, ground=True)
    L2, fms = L.mean(0), F.mean(0)
    return L2 / (1 - fms)


class MsTable:
    """Psi over (mus, h) on a grid, bilinear, as the GPU samples its 32x32 LUT"""
    def __init__(self, P, nu=32, nv=32, k=8, n=20):
        self.nu, self.nv = nu, nv
        self.t = np.zeros((nv, nu, 3))
        for y in range(nv):
            for x in range(nu):
                mus = (x + 0.5) / nu * 2 - 1
                h = (y + 0.5) / nv * (RT - RB)
                self.t[y, x] = ms_psi(P, RB + max(h, 0.01), mus, k, n)

    def __call__(self, r, mus):
        x = np.clip((mus * 0.5 + 0.5) * self.nu - 0.5, 0, self.nu - 1.001)
        y = np.clip((r - RB) / (RT - RB) * self.nv - 0.5, 0, self.nv - 1.001)
        x0, y0 = x.astype(int), y.astype(int)
        fx, fy = (x - x0)[:, None], (y - y0)[:, None]
        t = self.t
        return (t[y0, x0] * (1 - fx) * (1 - fy) + t[y0, x0 + 1] * fx * (1 - fy) + t[y0 + 1, x0] * (1 - fx) * fy
                + t[y0 + 1, x0 + 1] * fx * fy)


def view_dir(elev_deg, az_rel_deg):
    e, a = math.radians(elev_deg), math.radians(az_rel_deg)
    return np.array([math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)])


def sun_dir(elev_deg):
    e = math.radians(elev_deg)
    return np.array([math.cos(e), 0.0, math.sin(e)])


def sky(P, h_km, views, sun_elev, psi, n=40):
    return integrate(P, RB + h_km, np.array(views), sun_dir(sun_elev), n, psi=psi)[0]


# ---- the independent reference: backward Monte Carlo path tracing of the same medium, all orders
def path_trace(P, h_km, view, sun_elev, paths=20000, seed=1, max_bounce=40):
    rng = np.random.default_rng(seed)
    s = sun_dir(sun_elev)
    out = np.zeros(3)
    for c in range(3):
        sig_max = medium(P, np.linspace(0, RT - RB, 2001))[2][:, c].max() * 1.001
        pos = np.tile([0.0, 0.0, RB + h_km], (paths, 1))
        d = np.tile(view, (paths, 1)).astype(np.float64)
        w = np.ones(paths)
        alive = np.ones(paths, bool)
        acc = np.zeros(paths)
        for b in range(max_bounce):
            idx = np.nonzero(alive)[0]
            if idx.size == 0:
                break
            p, dd = pos[idx], d[idx]
            r = np.linalg.norm(p, axis=1)
            mu = (p * dd).sum(1) / r
            gnd = hits_ground(r, mu)
            dmax = np.where(gnd, dist_ground(r, mu), dist_top(r, mu))
            t = np.zeros(idx.size)
            event = np.zeros(idx.size, int)   # 0 pending, 1 collision, 2 escaped/ground
            pend = np.ones(idx.size, bool)
            while pend.any():
                k = np.nonzero(pend)[0]
                t[k] += -np.log(1 - rng.random(k.size)) / sig_max
                over = t[k] >= dmax[k]
                event[k[over]] = 2
                pend[k[over]] = False
                k2 = k[~over]
                q = p[k2] + dd[k2] * t[k2, None]
                h = np.linalg.norm(q, axis=1) - RB
                ext = medium(P, h)[2][:, c]
                real = rng.random(k2.size) < ext / sig_max
                event[k2[real]] = 1
                pend[k2[real]] = False
            # surface / escape
            k = np.nonzero(event == 2)[0]
            kg = k[gnd[k]]
            dead = k[~gnd[k]]
            alive[idx[dead]] = False
            if kg.size:
                q = p[kg] + dd[kg] * dmax[kg, None]
                n = q / np.linalg.norm(q, axis=1)[:, None]
                mus = (n * s).sum(1)
                Tg = transmittance(P, np.full(kg.size, RB + 1e-4), mus)[:, c]
                acc[idx[kg]] += w[idx[kg]] * Tg * np.maximum(mus, 0) * P['albedo'][c] / math.pi
                w[idx[kg]] *= P['albedo'][c]
                # cosine-weighted bounce
                u1, u2 = rng.random(kg.size), rng.random(kg.size)
                rr, ph = np.sqrt(u1), 2 * math.pi * u2
                tx = np.cross(n, np.where(np.abs(n[:, :1]) > 0.9, [[0, 1, 0]], [[1, 0, 0]]))
                tx /= np.linalg.norm(tx, axis=1)[:, None]
                ty = np.cross(n, tx)
                nd = tx * (rr * np.cos(ph))[:, None] + ty * (rr * np.sin(ph))[:, None] + n * np.sqrt(1 - u1)[:, None]
                pos[idx[kg]] = q + n * 1e-4
                d[idx[kg]] = nd
            # scattering
            k = np.nonzero(event == 1)[0]
            if k.size:
                q = p[k] + dd[k] * t[k, None]
                h = np.linalg.norm(q, axis=1) - RB
                rayS, mieS, ext = medium(P, h)
                sr, sm, se = rayS[:, c], mieS[:, c], ext[:, c]
                w[idx[k]] *= (sr + sm) / se
                up = q / np.linalg.norm(q, axis=1)[:, None]
                mus = (up * s).sum(1)
                Ts = transmittance(P, np.linalg.norm(q, axis=1), mus)[:, c]
                cs = (dd[k] * s).sum(1)
                ph = (sr * phase_r(cs) + sm * phase_hg(P['g'], cs)) / (sr + sm)
                acc[idx[k]] += w[idx[k]] * Ts * ph
                # new direction from the mixture
                choose_m = rng.random(k.size) < sm / (sr + sm)
                nd = np.zeros((k.size, 3))
                # HG about dd
                g = P['g']
                u = rng.random(k.size)
                cm = (1 + g * g - ((1 - g * g) / (1 - g + 2 * g * u)) ** 2) / (2 * g)
                # Rayleigh by rejection
                cr = np.zeros(k.size)
                todo = np.ones(k.size, bool)
                while todo.any():
                    j = np.nonzero(todo)[0]
                    c0 = 2 * rng.random(j.size) - 1
                    acc_ok = rng.random(j.size) < (1 + c0 * c0) / 2
                    cr[j[acc_ok]] = c0[acc_ok]
                    todo[j[acc_ok]] = False
                cth = np.where(choose_m, cm, cr)
                sth = np.sqrt(np.maximum(0, 1 - cth * cth))
                phi = 2 * math.pi * rng.random(k.size)
                a = dd[k]
                tx = np.cross(a, np.where(np.abs(a[:, :1]) > 0.9, [[0, 1, 0]], [[1, 0, 0]]))
                tx /= np.linalg.norm(tx, axis=1)[:, None]
                ty = np.cross(a, tx)
                nd = a * cth[:, None] + tx * (sth * np.cos(phi))[:, None] + ty * (sth * np.sin(phi))[:, None]
                pos[idx[k]] = q
                d[idx[k]] = nd
            # roulette
            if b > 4:
                lw = alive & (w < 0.05)
                kill = lw & (rng.random(paths) > 0.5)
                alive[kill] = False
                w[lw & ~kill] *= 2
        out[c] = acc.mean()
    return out


def rel(a, b):
    return np.abs(a - b) / np.maximum(np.abs(b), 1e-12)


def main():
    a = sys.argv[1:]
    red = a[a.index('--red') + 1] if '--red' in a else ''
    paths = int(a[a.index('--paths') + 1]) if '--paths' in a else 20000
    fails = 0

    def gate(name, ok, msg):
        nonlocal fails
        print(('PASS ' if ok else 'FAIL ') + name + ': ' + msg)
        fails += 0 if ok else 1

    P = params(ozone=red != 'noozone', ms=red != 'noms')
    # 1. optical depths at zenith vs published values (Bucholtz 1995 Rayleigh; ~300 DU ozone Chappuis band)
    od = -np.log(transmittance(P, np.array([RB]), np.array([1.0]), 4000))[0]
    ray_od = P['ray'] * P['rayH']
    gate('rayleigh-od', np.all(rel(ray_od, np.array([0.0444, 0.0973, 0.2370])) < 0.12),
         'model %s vs Bucholtz 1995 (680/550/440 nm) 0.0444/0.0973/0.2370' % np.round(ray_od, 4))
    gate('zenith-transmittance', abs(od[1] - (0.1085 + 0.0282 + 0.0053)) < 0.003,
         'od(550) %.4f = rayleigh 0.1085 + ozone 0.0282 + mie 0.0053' % od[1])

    # 2. Koschmieder: a weather whose fog reaches 0.98 at Day-Far 3 km has 2 % contrast at 3 km along the ground
    far_units = 3000.0 / UNIT_M
    hz = haze_from_fog(far_units, 1.0) if red != 'nohaze' else (0.0, 1.0, 0.0)
    Ph = params(haze=hz)
    # horizontal path at 2 m: the curvature is negligible over 3 km
    rr = RB + 0.002
    t = np.linspace(0, 3.0, 3001)
    hh = np.sqrt(rr * rr + t * t) - RB
    ext = medium(Ph, hh)[2][:, 1]
    contrast = math.exp(-np.trapz(ext, t))
    gate('koschmieder', abs(contrast - 0.02) < 0.002, 'contrast at Day-Far = %.4f (2 %% wanted), haze %.3f /km over %.3f km' % (contrast, hz[0], hz[1]))

    # 3. multiple scattering vs the path tracer at sun 30 and 10 deg: zenith and the horizon at 90 deg azimuth
    psi = MsTable(P, 32, 32)
    worst = 0.0
    lines = []
    for se in (30.0, 10.0):
        for (ve, va) in ((90.0, 0.0), (3.0, 90.0)):
            v = view_dir(ve, va)
            lut = sky(P, 0.1, [v], se, psi, 60)[0]
            ref = path_trace(params(), 0.1, v, se, paths)
            e = rel(lut, ref).max()
            worst = max(worst, e)
            lines.append('sun %g view %g/%g: model %s ref %s err %.3f' % (se, ve, va, np.round(lut, 5), np.round(ref, 5), e))
    for l in lines:
        print('   ', l)
    # Hillaire's isotropic 2nd-order-and-up term runs a few % low near the horizon at a low sun; MS off is ~50 % low
    gate('ms-vs-pathtracer', worst < 0.15, 'worst relative error %.3f (< 0.15)' % worst)

    # 4. ozone: the twilight zenith stays blue (blue / red ratio at sun -4 deg)
    psi_t = psi
    z = sky(P, 0.1, [view_dir(90, 0)], -4.0, psi_t, 60)[0]
    # Hulburt 1953: the Chappuis band, not Rayleigh, keeps the twilight zenith blue (without ozone B/G ~1.15)
    gate('twilight-ozone-blue', z[2] / z[1] > 2.0, 'zenith B/G at sun -4 deg = %.2f (> 2: the blue hour)' % (z[2] / z[1]))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
