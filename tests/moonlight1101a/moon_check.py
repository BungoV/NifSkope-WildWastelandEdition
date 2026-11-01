"""Lane FIX1101A (moon, LATE ITEMs 24b / 24c / 24d / 27f): the night key light of the cell view, on the twin
(moontwin.py) of gl/lookdevstage.cpp computeLight + gl/moonlight.h, Commonwealth (DefaultClimate) game day 0 =
2287-10-23, CommonwealthClear, the look env (Sunlight From Sky Full, Moon Phase and Stars on, our discs, Sun Path Real,
Real sun and moon off), Clear night haze 0.513 /km over 0.155 km (the atmo agent's 22:00 Clear frame).
Gates (stated before the twin ran):
  handover   over 16:00-21:30 and 04:30-08:00, 1-minute steps: the largest per-minute change of the light's luminance is
             no larger than the real sun's own (its part alone) x 1.05: the hand-over adds no step. Red: the x14 drop at
             the engine's D (19:04:30) and the ghost sun's rise.
  ghost      every minute with the real sun at -6 deg or lower and no moon up: no key light (luminance < 1e-6).
  anchor     the full moon at 45 deg = the record's Night key (1 %); over the night the light never exceeds the record x
             T(zenith)/T(45) (the air's own gain): no moon brighter than its record.
  phase      our globe's phase angle over game days 0..40 at 10-minute steps changes <= 0.5 deg a step (continuous); the
             red phase8 is the vanilla 8-step phase (45-degree steps at midnight every 4 days).
  engine     Sun Path = the engine clock (Full): the per-minute change at C..D, A..B <= x1.6 a minute (the 9-minute
             fade of the engine's own sun disc) where the red switches at D (x14 in one step).
  moon-fog   24b: Foggy 00:00, the moon through the frame's medium (h4p `cell air:` census: sigma0 3.88757e-05 /u, ground
             x5.667 over 4195.8 u): only the derived ground layer dims it (30dfaadd): the ground's direct moon >= 10 % of
             the record's. Red suncolumn: the whole column to the scene top (FOGSUN1).
  code       lookdevstage.cpp carries the rule the twin computes (wwmoon::combine, nightWeight, the elongation phase, the
             engine fade 1 - sunAlpha) and moonlight.h the same weights.
usage: python -I moon_check.py [--red handover|phase8|suncolumn] [--tables]   (exit 0 = all gates pass)"""
import sys, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import moontwin as M
TREE = os.path.dirname(os.path.dirname(HERE))
RED = sys.argv[sys.argv.index('--red') + 1] if '--red' in sys.argv else None
TABLES = '--tables' in sys.argv
HAZE = (0.513, 0.155, 0.0)
W = 'CommonwealthClear'
fails = 0


def gate(name, ok, msg):
    global fails
    print('%s %s: %s' % ('PASS' if ok else 'FAIL', name, msg))
    fails += 0 if ok else 1


night_lum = M.lum([M.lin(b) for b in M.KEYS[W][1]])
day_lum = M.lum([M.lin(b) for b in M.KEYS[W][0]])
hand = 'handover' if RED in ('handover', 'vanilla') else None


def series(t0, t1, fn):
    out = []
    n = int(round((t1 - t0) * 60))
    for i in range(n + 1):
        h = t0 + i / 60.0
        rgb, d, info = fn(h)
        out.append((h, M.lum(rgb), d, info))
    return out


def sun_only(h):
    s, e = M.real_sun(h, 0.0)
    rgb = M.full_key(s[2], HAZE, day_lum) if s[2] > 0 else (0.0, 0.0, 0.0)
    return rgb, s, dict(e_sun=e)


worst = (0.0, None)
worst_sun = 0.0
ghost = (0.0, None)
for t0, t1 in ((16.0, 21.5), (4.5, 8.0)):
    S = series(t0, t1, lambda h: M.light_after(h, 0.0, W, HAZE, red=hand))
    R = series(t0, t1, sun_only)
    for i in range(len(S) - 1):
        d = abs(S[i + 1][1] - S[i][1])
        if d > worst[0]:
            worst = (d, S[i][0])
        worst_sun = max(worst_sun, abs(R[i + 1][1] - R[i][1]))
    for h, L, d, info in S:
        e = info['e_sun']
        _, up = M.moon_geo_after(h, 0.0)
        if e <= -6.0 and not up and L > ghost[0]:
            ghost = (L, h)


def hm(h):
    return '%02d:%02d' % (int(h), int(round((h % 1) * 60)) % 60) if h is not None else '-'


gate('handover', worst[0] <= worst_sun * 1.05,
     'largest per-minute change %.5f at %s (the real sun alone: %.5f; record Night key %.4f)' % (worst[0], hm(worst[1]), worst_sun, night_lum))
gate('ghost', ghost[0] < 1e-6, 'key light with the sun <= -6 deg and no moon up: max %.5f at %s (Day key %.4f)' % (ghost[0], hm(ghost[1]), day_lum))
full45 = M.lum(M.full_key(0.70710678, HAZE, night_lum)) * M.ks(0.0)
gainz = M.lum(M.cpu_trans(1.0, HAZE)) / M.lum(M.cpu_trans(0.70710678, HAZE))
mx = max(M.lum(M.light_after(h / 60.0, 0.0, W, HAZE, red=hand)[0]) for h in range(int(19.5 * 60), 24 * 60, 10))
gate('anchor', abs(full45 / night_lum - 1.0) < 0.01 and mx <= night_lum * gainz * 1.0001,
     'full moon at 45 deg %.5f = record %.5f; night max %.5f <= record x T(zenith)/T(45) %.4f' % (full45, night_lum, mx, night_lum * gainz))
prev, worst_a, at = None, 0.0, None
for i in range(40 * 144 + 1):
    day, hour = divmod(i * 10.0 / 60.0, 24.0)
    a = M.alpha_vanilla(day) if RED in ('phase8', 'vanilla') else M.alpha_from_elong(M.elongation(day, hour))
    if prev is not None and abs(a - prev) > worst_a:
        worst_a, at = abs(a - prev), (day, hour)
    prev = a
gate('phase', worst_a <= 0.5, 'largest phase-angle step over 40 days at 10 min: %.3f deg (day %d %s)' % (worst_a, at[0], hm(at[1])))


def engine_light(h, red):
    """Sun Path = the engine clock, Full: the light is the engine's (floored) arc, the key crossfaded by 1 - sunAlpha"""
    c = M.clock(h)
    d = c['light']
    if red:
        kl = night_lum if c['night'] else day_lum
        f = M.ks(M.alpha_vanilla(0.0)) if c['night'] else 1.0
    else:
        wE = 1.0 - c['sunAlpha']
        nl = night_lum * M.ks(M.alpha_from_elong(M.elongation(0.0, h)))
        kl = day_lum * (nl / day_lum) ** wE   # geometric, lookdevstage.cpp std::pow( nightL / dayL, wE )
        f = 1.0
    return [v * f for v in M.full_key(d[2], HAZE, kl)], d, {}


E = series(18.5, 19.5, lambda h: engine_light(h, hand))
ratio = max(max(a[1], b[1]) / max(min(a[1], b[1]), 1e-9) for a, b in zip(E, E[1:]))
gate('engine', ratio <= 1.6, 'engine-clock path 18:30-19:30: largest per-minute ratio x%.3f (bar x1.6)' % ratio)

# 24b: the moon through the fog medium (tests/fogsun1101a's twin, the moon's mu at 00:00 day 0)
U_KM = 0.0142875e-3
FOG = dict(s0=3.88757e-05, zb=-9936.0, invH=1 / 789958.6, top=29811.8, s=5.667, Hg=4195.8, zg=64.0)
CLR = dict(s0=0.531 * U_KM, zb=-9936.0, invH=1 / 789958.6, top=29811.8, s=0.25, Hg=300.0, zg=64.0)


def col_one(lo, hi, z0, invH):
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


def Ts(c, mu, column):
    z = 64.0 + 1.7 * 70
    band = col_one(z, c['top'], c['zb'], c['invH'])
    grd = c['s'] * col_one(z, c['top'], c['zg'], 1.0 / c['Hg'])
    return math.exp(-c['s0'] * ((band + grd) if column else grd) / max(mu, 0.02))


_, d00, i00 = M.light_after(0.0, 0.0, W, HAZE)
mu00 = d00[2]
col = RED == 'suncolumn'
tf, tc = Ts(FOG, mu00, col), Ts(CLR, mu00, col)
gate('moon-fog', tf >= 0.10 and tf <= 1.0 and tc <= 1.0,
     'Foggy 00:00: the ground\'s direct moon %.1f %% of the record\'s (bar 10), Clear %.1f %% (moon elev %.1f deg)' % (100 * tf, 100 * tc, math.degrees(math.asin(mu00))))

lk = open(os.path.join(TREE, 'src/gl/lookdevstage.cpp'), encoding='latin-1').read()
mh = open(os.path.join(TREE, 'src/gl/moonlight.h'), encoding='latin-1').read()
gate('code', all(k in lk for k in ('wwmoon::combine( sun, sd, moon, md, wN, L.sun, dir )', 'wwmoon::nightWeight( float( eSun ) )',
                                   'elong = wwephem::elongationDeg(', '1.0f - raw.sunAlpha', 'moon[c] *= moonK * wN',
                                   'dayL * std::pow( std::max( nightL, 0.0f ) / dayL, wE )'))
     and 'std::clamp( -sunElevDeg / 6.0f, 0.0f, 1.0f )' in mh and 'o[2] = 0.02;' in mh,
     'lookdevstage.cpp computeLight / moonGeo and moonlight.h carry the rule the twin computes')

if '--dump' in sys.argv:
    # the C++ header itself (tests/moonlight1101a/moonlight_dump.cpp) against this twin
    import subprocess
    out = subprocess.run([sys.argv[sys.argv.index('--dump') + 1]], capture_output=True, text=True, timeout=60).stdout.split('\n')
    cases = [((0.3, 0.28, 0.22), (0.6, -0.5, 0.2), (0, 0, 0), (-0.6, -0.5, 0.3), 0.0),
             ((0, 0, 0), (0.9, 0.1, -0.1), (0.02, 0.017, 0.013), (-0.3, -0.5, 0.8), 1.0),
             ((0.01, 0.008, 0.006), (0.9, 0.1, 0.05), (0.02, 0.017, 0.013), (-0.3, -0.5, 0.8), 0.4),
             ((0, 0, 0), (0.9, 0.1, -0.2), (0, 0, 0), (-0.6, -0.5, 0.1), 0.7),
             ((0, 0, 0), (0.9, 0.1, -0.2), (0, 0, 0), (-0.6, -0.5, 0.1), 0.2)]
    err = 0.0
    for i, (s, sd, m, md, w) in enumerate(cases):
        rgb, d = M.combine(list(s), M.norm(list(sd)), list(m), M.norm(list(md)), w)
        got = [float(x) for x in out[i].split()]
        err = max(err, max(abs(a - b) for a, b in zip(rgb + d, got)))
    k = len(cases)
    for e in (5.0, 0.0, -1.0, -3.0, -5.5, -6.0, -20.0):
        err = max(err, abs(M.night_weight(e) - float(out[k].split()[2])))
        k += 1
    for e in (0.0, 45.0, 90.0, 160.0, 180.0, 200.0, 300.0):
        err = max(err, abs(M.alpha_from_elong(e) - float(out[k].split()[2])))
        k += 1
    gate('header', err < 2e-5, 'gl/moonlight.h (C++ dump) vs the twin: max abs diff %.2e' % err)

if TABLES:
    print('\n# 24b: the moon through the air at 00:00 (day 0), moon elev %.2f deg (old light: the engine arc floored)' % math.degrees(math.asin(mu00)))
    _, dold, _ = M.light_before(0.0, 0.0, W, HAZE)
    for name, c in (('Foggy', FOG), ('Clear', CLR)):
        for lab, mu in (('new (our globe)', mu00), ('old (engine arc)', dold[2])):
            print('  %-5s %-17s mu %.3f  Ts ground layer %.4f  whole column (red suncolumn) %.4f' % (name, lab, mu, Ts(c, mu, False), Ts(c, mu, True)))
    print('\n# 24c: moon vs noon sun (physical, cited) and the cell view (scene units), day 0 Clear')
    FC = 10.7639
    e_full = 10 ** (-0.4 * (-12.73 + 16.57)) * FC
    e_sun = 10 ** (-0.4 * (-26.74 + 16.57)) * FC
    print('  above the air: full moon %.4f lx (K&S 1991 eqs. 8-9), sun %.0f lx (V -26.74, NASA; same eq. 8), ratio %.3e;'
          ' albedo check p (R/d)^2 = 0.12 x (1737.4/384400)^2 = %.3e' % (e_full, e_sun, e_full / e_sun, 0.12 * (1737.4 / 384400.0) ** 2))
    sN, eN = M.real_sun(12.0, 0.0)
    Tn = M.lum(M.cpu_trans(sN[2], HAZE))
    sun_h = e_sun * Tn * sN[2]
    print('  noon sun day 0: elev %.2f deg, T %.3f, horizontal %.0f lx' % (eN, Tn, sun_h))
    for el in (60.0, 45.0, 30.0):
        mu = math.sin(math.radians(el))
        Tm = M.lum(M.cpu_trans(mu, HAZE))
        T45 = M.lum(M.cpu_trans(0.70710678, HAZE))
        for name, a in (('full', 0.0), ('gibbous', 45.0), ('quarter', 90.0), ('crescent', 135.0), ('new', 180.0)):
            phys = e_full * M.ks(a) * Tm * mu
            cell = night_lum * M.ks(a) * Tm / T45 * mu
            celln = day_lum * Tn / T45 * sN[2]
            print('  moon %2d deg %-8s alpha %5.1f KS %.4f: %.4f lx horiz = %.2e of noon sun | cell view: %.5f = %.2e of its noon sun (x%.0f, %.1f EV)' % (
                el, name, a, M.ks(a), phys, phys / sun_h, cell, cell / celln, (cell / celln) / (phys / sun_h), math.log2((cell / celln) / (phys / sun_h))))
    print('  Kyba, Mohar & Posch 2017 (A&G 58(1) 1.31): measured 0.26 lx horizontal at 55.5 deg, ~0.32 lx at the zenith')
    print('\n# full / quarter moon days (ephemeris elongation at 00:00; phase angle alpha)')
    for day in range(0, 40):
        a = M.alpha_from_elong(M.elongation(day, 0.0))
        if a < 8 or abs(a - 90) < 7:
            print('  WW_LOOKDEV_DAY=%d  alpha %.1f deg  KS %.3f  %s' % (day, a, M.ks(a), 'waxing' if M.elongation(day, 0.0) < 180 else 'waning'))
print('%s: %d gate(s) failed%s' % ('GATE OK' if not fails else 'GATE FAIL', fails, (' (red %s)' % RED) if RED else ''))
sys.exit(1 if fails else 0)
