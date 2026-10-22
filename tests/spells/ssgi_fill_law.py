"""Offline twin of the SSGI fill law (lane SSGI3; res/shaders/cell_ssgi.frag cast(), cell_lights.glsl cellSsgiE).
One slice of cast() replayed in a 2D view-space scene (camera at the origin looking +x, screen = y / x), then
E = E_probe x open + E_screen as the cell shaders apply it. Two scenes, bars stated before measuring:
  furnace  every surface's radiance = the probe's (L = 1): E must equal E_probe within 1 %
  corner   floor receiver, a lit wall well inside the radius, probe = sky only: E vs the exact slice integral within 10 %
           (DC2's bar). The exact answer: pi x (L_sky x open sky share + L_wall x the block's share), cosine-weighted.
The fill law must PASS both. The SSGI1 law (red `oldlaw`: donor cosine on the light, range band on the light only,
back faces shut sectors with no light) must FAIL the furnace (the invariant); its corner number is reported only (a
block facing the receiver at close range is its easy case; the live gate DC2 measures the real near field).  USAGE  python ssgi_fill_law.py   (exit 0 = verdict as expected)"""
import math, sys

PI = math.pi
RADIUS, THICK, STEPS, FOV, HPX = 256.0, 32.0, 32, 70.0, 1080
FLOOR_Y, WALL_X, TOP_Y, REC_X = -100.0, 500.0, -20.0, 420.0	# a block x >= 500, top below the eye


def hit(s):
    """first surface along the camera ray (1, s): position, normal; None = sky. Floor y = FLOOR_Y up to the block,
    the block's front x = WALL_X up to TOP_Y, its top y = TOP_Y beyond (the eye sees it, so the slab rule is exact)"""
    if s < 0.0 and FLOOR_Y / s <= WALL_X:
        return (FLOOR_Y / s, FLOOR_Y), (0.0, 1.0)
    if WALL_X * s <= TOP_Y:
        return (WALL_X, WALL_X * s), (-1.0, 0.0)
    if s < 0.0:
        return (TOP_Y / s, TOP_Y), (0.0, 1.0)
    return None


def norm(v):
    l = math.hypot(*v)
    return (v[0] / l, v[1] / l)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1]


def sector_bits(a0, a1):
    b0 = int(math.floor(min(max(a0, 0.0), 1.0) * 32.0))
    b1 = int(math.ceil(min(max(a1, 0.0), 1.0) * 32.0))
    return 0 if b1 <= b0 else ((1 << (b1 - b0)) - 1) << b0


def slice_cast(radiance, law, jitter):
    """one slice of cast(): returns (E_screen / pi, open share)"""
    P = (REC_X, FLOOR_Y)
    N = (0.0, 1.0)
    V = norm((-P[0], -P[1]))
    up = (0.0, 1.0)
    o = norm((up[0] - V[0] * dot(up, V), up[1] - V[1] * dot(up, V)))
    n = math.copysign(1.0, dot(N, o)) * math.acos(max(-1.0, min(1.0, dot(N, V))))
    f = 0.5 * HPX / math.tan(math.radians(FOV) / 2)  # pixels per unit of screen slope
    s0 = P[1] / P[0]
    rpx = RADIUS * f / P[0]
    occ, sil, taken = 0, 0.0, 0.0
    for sgn in (1.0, -1.0):
        for k in range(STEPS):
            t = (k + jitter) / STEPS
            off = max(t * t * rpx, 1.0 + k)
            h = hit(s0 + sgn * off / f)
            if h is None:
                continue
            S, Ns = h
            dF = (S[0] - P[0], S[1] - P[1])
            ln = math.hypot(*dF)
            if ln > RADIUS or ln < 1e-3:
                continue
            dB = (dF[0] - V[0] * THICK, dF[1] - V[1] * THICK)
            aF = sgn * math.acos(max(-1.0, min(1.0, dot((dF[0] / ln, dF[1] / ln), V))))
            aB = sgn * math.acos(max(-1.0, min(1.0, dot(norm(dB), V))))
            uF = 0.5 + 0.5 * math.sin(max(-PI / 2, min(PI / 2, aF - n)))
            uB = 0.5 + 0.5 * math.sin(max(-PI / 2, min(PI / 2, aB - n)))
            bits = sector_bits(min(uF, uB), max(uF, uB))
            fresh = bits & ~occ
            if fresh:
                rt = max(0.0, min(1.0, (RADIUS - ln) / max(0.25 * RADIUS, 1.0)))
                band = rt * rt * (3.0 - 2.0 * rt)
                cnt = bin(fresh).count('1') / 32.0
                facing = max(0.0, min(1.0, dot(Ns, (-dF[0] / ln, -dF[1] / ln))))
                L = radiance(Ns)
                if law == 'old':
                    sil += cnt * band * facing * L
                else:
                    front = max(0.0, min(1.0, facing / 0.1))
                    sil += cnt * band * front * L
                    taken += cnt * band * front
                occ |= bits
    openv = 1.0 - bin(occ).count('1') / 32.0 if law == 'old' else 1.0 - taken
    return sil, openv


def run(law, L_probe, radiance):
    js = [(i + 0.5) / 16 for i in range(16)]
    es = [slice_cast(radiance, law, j) for j in js]
    sil = sum(e[0] for e in es) / len(es)
    op = sum(e[1] for e in es) / len(es)
    return PI * (L_probe * op + sil)


def exact_corner(L_sky, L_wall):
    # cosine-weighted 2D share (uniform in sin of the angle from N) of the wall seen from the receiver
    top = math.atan2(WALL_X - REC_X, TOP_Y - FLOOR_Y)	# the block's front edge, its angle from N
    share = 0.5 * (1.0 - math.sin(top))				# from there down to the floor's horizon
    return PI * (L_sky * (1.0 - share) + L_wall * share)


def main():
    rows, ok = [], True
    for law in ('fill', 'old'):
        f = run(law, 1.0, lambda Ns: 1.0) / PI
        c_sky, c_wall = 0.2, 1.0
        c = run(law, c_sky, lambda Ns: c_wall) / exact_corner(c_sky, c_wall)
        pf, pc = abs(f - 1.0) <= 0.01, abs(c - 1.0) <= 0.10
        want = law == 'fill'
        good = (pf == want) and (pc or not want)
        ok = ok and good
        rows.append('law %-4s furnace E/E_probe %.4f %s | corner E/exact %.4f %s | expected %s -> %s' % (
            law, f, 'PASS' if pf else 'FAIL', c, 'PASS' if pc else 'FAIL', 'PASS' if want else 'FAIL',
            'OK' if good else 'WRONG'))
        rows[-1] = rows[-1].replace('expected FAIL', 'expected furnace FAIL')
    print('\n'.join(rows))
    print('VERDICT', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
