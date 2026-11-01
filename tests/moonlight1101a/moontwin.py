"""Lane FIX1101A (moon): a twin of the cell view's key light round the clock (gl/lookdevstage.cpp computeLight under the
look env: Sunlight From Sky = Full, Moon Phase and Stars on, our discs, Sun Path = Real, Real sun and moon OFF) and of the
new night rule (gl/moonlight.h). Pure Python, no NifSkope. Imported by moon_check.py.

Inputs (MEASURED from Fallout4.esm by notes/fix1101a/moon/tools/esmclim.py + clmtdbg.py, read-only):
  Commonwealth WRLD CNAM -> CLMT 0000015F DefaultClimate TNAM 30,54,102,126,175,68 (sunrise 5:00-9:00, sunset 17:00-21:00,
  moons 68 = Masser + 4-day phases); GMST fSunXExtreme 600, fSunYExtreme -325, fSunAlphaTransTime 0.15,
  fDaytimeColorExtension 2.0, fSunShadowScale -15 (fSunShadowMinAngle not in the ESM: the exe's 30);
  WTHR CommonwealthClear 0002B52A Sunlight Day 225,225,225 Night 53,70,87; CommonwealthFoggy 001C3473 Day 255,255,255
  Night 50,67,85."""
import math

KRB, KRT = 6360.0, 6460.0
KRAY = (5.802e-3, 13.558e-3, 33.1e-3)
KMIES, KMIEE = 3.996e-3, 4.40e-3
KOZ = (0.650e-3, 1.881e-3, 0.085e-3)
DEG = math.pi / 180.0
ENG_DEG = 0.0174532924
TNAM = (30, 54, 102, 126)
MOONS = 68
GM = dict(sunX=600.0, sunY=-325.0, alphaTrans=0.15, colorExt=2.0, shadowScale=-15.0, shadowMin=30.0)
KEYS = {'CommonwealthClear': ((225, 225, 225), (53, 70, 87)), 'CommonwealthFoggy': ((255, 255, 255), (50, 67, 85))}


def lin(b):
    return min(max(b / 255.0, 0.0), 1.0) ** 2.2


def lum(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def od_at(h, haze):
    h = max(h, 0.0)
    dr, dm = math.exp(-h / 8.0), math.exp(-h / 1.2)
    dz = max(0.0, 1.0 - abs(h - 25.0) / 15.0)
    dh = haze[0] * math.exp(-max(h - haze[2], 0.0) / max(haze[1], 0.02))
    return [KRAY[c] * dr + KMIEE * dm + dh + KOZ[c] * dz for c in range(3)]


_TC = {}


def cpu_trans(mu, haze):
    """atmomath.h cpuTrans from the ground (h 0), 256 steps, ozone x 1"""
    key = (round(mu, 6), tuple(haze))
    if key in _TC:
        return _TC[key]
    r = KRB
    if mu < 0.0 and r * r * (mu * mu - 1.0) + KRB * KRB >= 0.0:
        return (0.0, 0.0, 0.0)
    d = max(0.0, -r * mu + math.sqrt(max(0.0, r * r * (mu * mu - 1.0) + KRT * KRT)))
    N = 256
    od = [0.0, 0.0, 0.0]
    for i in range(N):
        a0, a1 = i / N, (i + 1) / N
        am = 0.5 * (a0 + a1)
        t, w = d * am * am, d * (a1 * a1 - a0 * a0)
        rr = math.sqrt(r * r + t * t + 2.0 * r * mu * t)
        e = od_at(rr - KRB, haze)
        for c in range(3):
            od[c] += e[c] * w
    out = tuple(math.exp(-o) for o in od)
    _TC[key] = out
    return out


def full_key(mu, haze, key_lum45):
    """atmomath.h sunLight mode 2: k x T(mu), k = key luminance / lum(T45)"""
    T45 = cpu_trans(0.70710678, haze)
    k = key_lum45 / max(1e-4, lum(T45))
    T = cpu_trans(mu, haze)
    return tuple(k * t for t in T)


def ks(alpha):
    a = abs(alpha)
    return 10.0 ** (-0.4 * (0.026 * a + 4e-9 * a ** 4))


# ---- the engine clock (esmweather.cpp wwSkyClock) ----
def clock(t):
    srMid = (TNAM[0] + TNAM[1]) / 12.0
    ssMid = (TNAM[2] + TNAM[3]) / 12.0
    hh = GM['alphaTrans'] * 0.5
    A, B, C, D = srMid - hh, srMid + hh, ssMid - hh, ssMid + hh
    if t < A or t > D:
        alpha = 0.0
    elif t < B:
        alpha = (t - A) / max(1e-9, B - A)
    elif t <= C:
        alpha = 1.0
    else:
        alpha = 1.0 - (t - C) / max(1e-9, D - C)
    if A <= t <= D:
        x = 1.0 - 2.0 * (t - A) / max(1e-9, D - A)
    else:
        tt = t - D if t >= D else 24.0 - D + t
        x = 2.0 * tt / max(1e-9, 24.0 - (D - A)) - 1.0
    X = GM['sunX']
    pos = (x * X, GM['sunY'], abs(X) - abs(x * X))
    l = math.sqrt(sum(p * p for p in pos)) or 1.0
    nx, ny = pos[0] / l, pos[1] / l
    z = pos[2] / l + GM['shadowScale'] * ENG_DEG
    z = max(z, GM['shadowMin'] * ENG_DEG)
    l2 = math.sqrt(nx * nx + ny * ny + z * z)
    light = (nx / l2, ny / l2, z / l2)
    return dict(A=A, B=B, C=C, D=D, sunAlpha=alpha, pos=pos, light=light, night=(t < A or t > D))


# ---- Sun Path = Real, no ephemeris (shadowset.cpp wwSunPathReal) ----
def real_sun(hour, day):
    n = math.fmod(296.0 + math.floor(max(day, 0.0)), 365.0) + 1.0
    decl = 23.44 * DEG * math.sin(2.0 * math.pi * (284.0 + n) / 365.0)
    H = (hour - 12.0) * 15.0 * DEG
    phi = 42.36 * DEG
    o = [-math.cos(decl) * math.sin(H),
         math.cos(phi) * math.sin(decl) - math.sin(phi) * math.cos(decl) * math.cos(H),
         math.sin(phi) * math.sin(decl) + math.cos(phi) * math.cos(decl) * math.cos(H)]
    l = math.sqrt(sum(v * v for v in o))
    o = [v / l for v in o]
    return o, math.asin(max(-1.0, min(1.0, o[2]))) / DEG


def norm(v):
    l = math.sqrt(sum(x * x for x in v))
    return [x / l for x in v] if l > 1e-12 else None


def slerp(a, b, w):
    c = max(-1.0, min(1.0, sum(x * y for x, y in zip(a, b))))
    om = math.acos(c)
    if om < 1e-4 or om > math.pi - 1e-3:
        o = norm([x * (1 - w) + y * w for x, y in zip(a, b)])
        return o if o else list(b)
    s = math.sin(om)
    return norm([x * math.sin((1 - w) * om) / s + y * math.sin(w * om) / s for x, y in zip(a, b)])


def sunsync(hour, day, engine_light):
    """wwSunPathApply under Sun Path = Real: the real sun above the horizon; below it, the light crossfades from the
    real sun's horizontal direction to the engine's light by -6 degrees"""
    r, e = real_sun(hour, day)
    if e >= 0.0:
        return r, e
    rh = norm([r[0], r[1], 0.0]) or [0.0, 0.0, 1.0]
    w0 = min(max(-e / 6.0, 0.0), 1.0)
    w = w0 * w0 * (3.0 - 2.0 * w0)
    return slerp(rh, list(engine_light), w), e


# ---- skyephem.h: the game date and the sun-moon elongation (0 new, 180 full) ----
DIM = (0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)


def game_date(day, hour):
    y, m, d = 2287, 10, 23
    n = int(math.floor(max(day, 0.0)))
    while n > 0:
        left = DIM[m] - d
        if n <= left:
            d += n
            n = 0
        else:
            n -= left + 1
            d = 1
            m += 1
            if m > 12:
                m = 1
                y += 1
    return y, m, d, math.fmod(hour, 24.0)


def julian_day(year, month, dayfrac):
    y, m = float(year), float(month)
    if m <= 2:
        y -= 1
        m += 12
    A = math.floor(y / 100.0)
    B = 2.0 - A + math.floor(A / 4.0)
    return math.floor(365.25 * (y + 4716.0)) + math.floor(30.6001 * (m + 1.0)) + dayfrac + B - 1524.5


def sun_app_lon(t):
    L0 = math.fmod(280.46646 + t * (36000.76983 + t * 0.0003032), 360.0)
    M = 357.52911 + t * (35999.05029 - 0.0001537 * t)
    C = math.sin(M * DEG) * (1.914602 - t * (0.004817 + 0.000014 * t)) + math.sin(2 * M * DEG) * (0.019993 - 0.000101 * t) \
        + math.sin(3 * M * DEG) * 0.000289
    return L0 + C - 0.00569 - 0.00478 * math.sin((125.04 - 1934.136 * t) * DEG)


def elongation(day, hour, lon=-71.06):
    y, m, d, h = game_date(day, hour)
    jd = julian_day(y, m, d + (h - lon / 15.0) / 24.0)
    dd = jd - 2451545.0
    Lp = 218.3164477 + 13.17639648 * dd
    M = 134.9633964 + 13.06499295 * dd
    Ms = 357.5291092 + 0.98560028 * dd
    De = 297.8501921 + 12.19074912 * dd
    F = 93.2720950 + 13.22935024 * dd
    lonm = Lp + 6.289 * math.sin(M * DEG) + 1.274 * math.sin((2 * De - M) * DEG) + 0.658 * math.sin(2 * De * DEG) \
        + 0.214 * math.sin(2 * M * DEG) - 0.186 * math.sin(Ms * DEG) - 0.114 * math.sin(2 * F * DEG)
    lonm = math.fmod(lonm, 360.0)
    e = math.fmod(lonm - sun_app_lon((jd - 2451545.0) / 36525.0), 360.0)
    return e + 360.0 if e < 0 else e


def alpha_from_elong(e):
    """our globe's phase angle for a sun placed at elongation e from the moon (skydisc.h sunFromElongation +
    phaseAngleDeg): 180 - acos(cos e) = |180 - e| folded to 0..180"""
    return 180.0 - math.acos(max(-1.0, min(1.0, math.cos(e * DEG)))) / DEG


def alpha_vanilla(day):
    L = MOONS & 0x3F
    p = (int(math.floor(max(0.0, day))) % (8 * L)) // L
    return alpha_from_elong(180.0 + 45.0 * p)


# ---- the two rules ----
def night_weight(e_sun):
    """moonlight.h nightWeight: the night light's share by the sun's elevation, 0 at the horizon, 1 by -6 deg (civil
    dusk), smoothstep (the crossfade SunSync's direction already used)"""
    w0 = min(max(-e_sun / 6.0, 0.0), 1.0)
    return w0 * w0 * (3.0 - 2.0 * w0)


def light_before(hour, day, weather, haze):
    """today: computeLight under the look env (engine clock + SunSync direction, Full keyed Day/Night at the engine's
    A/D, the phase factor at night)"""
    day_key, night_key = KEYS[weather]
    c = clock(hour)
    d, e = sunsync(hour, day, c['light'])
    key = night_key if c['night'] else day_key
    kl = lum([lin(b) for b in key])
    rgb = list(full_key(d[2], haze, kl))
    if c['night']:
        f = ks(alpha_vanilla(day))
        rgb = [v * f for v in rgb]
    return rgb, d, dict(e_sun=e, night=c['night'])


def moon_geo_after(hour, day):
    """the moon our globe is drawn at (Real sun and moon off): the engine's night point, up only by the engine's night"""
    c = clock(hour)
    D = norm(list(c['pos']))
    up = c['night'] and D is not None and D[2] > 0.0
    return D, up


def light_after(hour, day, weather, haze, red=None):
    """the new rule (moonlight.h wwmoon::combine): sun part = the Day key through the air toward the REAL sun (0 under
    the horizon); moon part = the Night key (the record = the full moon at 45 deg) x KS(our globe's phase angle, from the
    ephemeris elongation) through the air toward our globe, x the night weight; one light = their sum, its direction
    the luminance-weighted slerp of the two; z held at 0.02 or above"""
    if red:
        return light_before(hour, day, weather, haze)
    day_key, night_key = KEYS[weather]
    s, e = real_sun(hour, day)
    sun = list(full_key(s[2], haze, lum([lin(b) for b in day_key]))) if s[2] > 0.0 else [0.0, 0.0, 0.0]
    D, up = moon_geo_after(hour, day)
    alpha = alpha_from_elong(elongation(day, hour))
    wN = night_weight(e)
    moon = [0.0, 0.0, 0.0]
    if up and wN > 0.0:
        m = full_key(D[2], haze, lum([lin(b) for b in night_key]))
        f = ks(alpha) * wN
        moon = [v * f for v in m]
    rgb, d = combine(sun, s, moon, D if D else s, wN)
    return rgb, d, dict(e_sun=e, alpha=alpha, wN=wN, up=up, sun=sun, moon=moon)


def air_col_one(lo, hi, z0, invH):
    """lookdev_fog.glsl wwAirColOne / moonlight.h airColOne"""
    if hi <= lo:
        return 0.0
    if invH <= 0:
        return hi - lo
    below = max(min(hi, z0) - lo, 0.0)
    a = max(lo, z0)
    if hi <= a:
        return below
    w = hi - a
    u = w * invH
    e1 = (1 - math.exp(-u)) / u if u > 1e-3 else 1 - u * (0.5 - u / 6)
    return below + math.exp(-(a - z0) * invH) * w * e1


def air_band_t(zc, mu, med, layer):
    c = air_col_one(zc, med[3], med[1], med[2])
    if layer[0] > 0:
        c = (1 - layer[0]) * c + layer[0] * air_col_one(zc, med[3], layer[1], layer[2])
    return math.exp(-med[0] * c / max(mu, 0.02))


def air_layer_t(zc, mu, med, layer, ground):
    if zc >= med[3] or layer[3] <= 0:
        return 1.0
    return math.exp(-med[0] * layer[3] * air_col_one(zc, med[3], ground[0], ground[1]) / max(mu, 0.02))


def disc_air_gain(zc, mu, t_sky, med, layer, ground):
    """moonlight.h discAirGain: the disc's gain so that, after the air's composite clamp(T_band T_layer / T_sky), it
    ends at T_layer of its drawn value, as its light"""
    if not med[0] > 0 or mu <= 0:
        return 1.0
    tb, tl = air_band_t(zc, mu, med, layer), air_layer_t(zc, mu, med, layer, ground)
    return min(max(max(tl, t_sky / max(tb, 1e-6)), 0.0), 1e4)


def combine(sun, sdir, moon, mdir, wN):
    ls, lm = lum(sun), lum(moon)
    rgb = [a + b for a, b in zip(sun, moon)]
    if ls + lm > 1e-12:
        share = lm / (ls + lm)
    else:
        share = 1.0 if wN >= 0.5 else 0.0
    d = slerp(list(sdir), list(mdir), share)
    if d[2] < 0.02:
        h = norm([d[0], d[1], 0.0]) or [1.0, 0.0, 0.0]
        c = math.sqrt(1.0 - 0.02 * 0.02)
        d = [h[0] * c, h[1] * c, 0.02]
    return rgb, d
