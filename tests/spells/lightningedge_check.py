"""Lane LIGHTNING2 (LIGHTNINGEDGE1, CLOUD6f leftover): the strike's field near a cloud's SIDE and in thin wisps, against a
heterogeneous Monte Carlo. CLOUD6f's field (src/gl/cloudlightning.h fieldAt, GLSL twins cloud_composite.frag cvLtField
and lightning_field.glsl, GPU == twin by cloudlightning_check F / lightning2_check K) has image planes only at the Low
band's base and top; the composite evaluates it at the clouds' mean depth x the opacity. No NifSkope, no GL draw (the
checker imports cloudvol_check for the droplet phase and the mean cosine only).

    python -I tests/spells/lightningedge_check.py <scratch dir> [--only ABC] [--photons N]

Scene (pre-registered, LIGHTNING2 2026-10-31): the Low band 1.5 .. 3.0 km at density 1 (sigma 40 / km, albedo 0.99, the
droplet phase) for x < 0 only (a cloud SIDE at x = 0, infinite in y and toward -x); the strike 0.3 km inside the side at
35 % of the depth (x -0.3, z 2.025); a thin wisp beside it: a horizontal sheet z 2.000 .. 2.050, x 0.2 .. 4 km at
sigma 20 / km (tau 1 through it vertically). An isotropic point source; delta tracking (majorant 40 / km); radiance per
unit source power from the outward crossings of a tally plane within a cone (cos >= 0.97) per bin area (|y| <= 0.15 km
for A and B, 0.3 km for C).
Gates (bars as CLOUD6f's gate L, 0.5 .. 2.0 x the Monte Carlo per bin with >= 400 photons; fewer = INFO):
  A base   the base (z 1.5) seen straight up at x -1.0, -0.6, -0.3, -0.1: model = fieldAt at the base + one extinction
           length, d up (the composite in thick cloud, opacity 1).
  B side   the side (x 0) seen along -x at z 1.75, 2.025, 2.6: model = fieldAt one extinction length inside, d = -x.
  C wisp   the wisp seen straight up from below at x 0.5, 1.0, 2.0: model = (1 - e^-1) x fieldAt at the wisp's middle
           (the band's field: the composite knows the band's density, not the wisp's).
Exit 0 only when every gate run passes.
"""
import math, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'lightningedge_scratch')
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ABC')
NPH = int(args[args.index('--photons') + 1]) if '--photons' in args else 1000000
red = args[args.index('--red') + 1] if '--red' in args else ''
os.makedirs(scratch, exist_ok=True)
sys.path.insert(0, HERE)
sys.argv = [sys.argv[0], scratch, '--only', '-']
import cloudvol_check as K   # noqa: E402
import numpy as np            # noqa: E402

GPP = 'C:/msys64/ucrt64/bin/g++.exe'
ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
DUMP = os.path.join(scratch, 'lightning2_dump.exe')
_r = subprocess.run([GPP, '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                     os.path.join(TREE, 'tests', 'lightning2', 'lightning2_dump.cpp'), '-o', DUMP], env=ENV,
                    capture_output=True, text=True)
if _r.returncode:
    raise SystemExit('BUILD FAIL ' + _r.stderr[-2000:])

SIGMA, ALB = 40.0, 0.99
BASE, TOP = 1.5, 3.0
SRC = (-0.3, 0.0, BASE + 0.35 * (TOP - BASE))
WZ0, WZ1, WX0, WX1, WSIG = 2.000, 2.050, 0.2, 4.0, 20.0
XMIN = -8.0
CONE = 0.97
results = []


def gate(name, ok, msg):
    results.append(ok)
    print('%-10s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg), flush=True)


def sigma_at(p):
    s = np.zeros(len(p))
    cloud = (p[:, 0] < 0.0) & (p[:, 2] > BASE) & (p[:, 2] < TOP)
    s[cloud] = SIGMA
    wisp = (p[:, 0] > WX0) & (p[:, 0] < WX1) & (p[:, 2] > WZ0) & (p[:, 2] < WZ1)
    s[wisp] = WSIG
    return s


def mc(n, seed=11):
    """outward plane crossings (x, y, z, dir) of the three tallies, with weights"""
    rng = np.random.default_rng(seed)
    cs = np.linspace(-1, 1, 20001)
    pdf = K.droplet(K.drop, cs) * 2 * math.pi
    cdf = np.concatenate([[0], np.cumsum((pdf[1:] + pdf[:-1]) / 2 * np.diff(cs))])
    cdf /= cdf[-1]
    pos = np.tile(np.array(SRC, float), (n, 1))
    u = rng.random(n) * 2 - 1
    ph = rng.random(n) * 2 * math.pi
    d = np.stack([np.sqrt(1 - u * u) * np.cos(ph), np.sqrt(1 - u * u) * np.sin(ph), u], 1)
    w = np.ones(n)
    alive = np.arange(n)
    tA, tB, tC = [], [], []   # (coord, weight) per tally
    for it in range(200000):
        if alive.size == 0:
            break
        P, Dd, Wt = pos[alive], d[alive], w[alive]
        s = -np.log(1 - rng.random(alive.size)) / SIGMA   # the majorant
        Q = P + Dd * s[:, None]
        # tally A: down through z = BASE at x < 0 (the cloud's base, the camera below)
        cr = (P[:, 2] > BASE) & (Q[:, 2] <= BASE) & (Dd[:, 2] <= -CONE)
        if cr.any():
            t = (BASE - P[cr, 2]) / Dd[cr, 2]
            xy = P[cr, :2] + Dd[cr, :2] * t[:, None]
            m = xy[:, 0] < 0.0
            tA.append(np.stack([xy[m, 0], xy[m, 1], Wt[cr][m]], 1))
        # tally B: out through x = 0 (+x) between the base and the top
        cr = (P[:, 0] < 0.0) & (Q[:, 0] >= 0.0) & (Dd[:, 0] >= CONE)
        if cr.any():
            t = (0.0 - P[cr, 0]) / Dd[cr, 0]
            yz = P[cr][:, 1:] + Dd[cr][:, 1:] * t[:, None]
            m = (yz[:, 1] > BASE) & (yz[:, 1] < TOP)
            tB.append(np.stack([yz[m, 0], yz[m, 1], Wt[cr][m]], 1))
        # tally C: down through z = WZ0 at x > WX0 (the wisp's underside)
        cr = (P[:, 2] > WZ0) & (Q[:, 2] <= WZ0) & (Dd[:, 2] <= -CONE)
        if cr.any():
            t = (WZ0 - P[cr, 2]) / Dd[cr, 2]
            xy = P[cr, :2] + Dd[cr, :2] * t[:, None]
            m = xy[:, 0] > WX0
            tC.append(np.stack([xy[m, 0], xy[m, 1], Wt[cr][m]], 1))
        # leave the world: below the wisp and the base with no way back up there is nothing, above the top, far -x
        gone = (Q[:, 2] < min(BASE, WZ0) - 1e-6) | (Q[:, 2] > TOP) | (Q[:, 0] < XMIN) | (Q[:, 0] > WX1 + 1.0) | \
               ((Q[:, 0] > 0.0) & ((Q[:, 2] < WZ0) | (Q[:, 2] > WZ1)) & (Q[:, 0] > WX1))
        # a photon outside every medium flies on: one more flight at the majorant is a null collision
        sg = sigma_at(Q)
        real = (rng.random(alive.size) < sg / SIGMA) & ~gone
        pos[alive] = Q
        idx = alive[real]
        w[idx] *= ALB
        # new directions at the real collisions
        ct = np.interp(rng.random(idx.size), cdf, cs)
        phi = rng.random(idx.size) * 2 * math.pi
        st_ = np.sqrt(np.maximum(0, 1 - ct * ct))
        dd = d[idx]
        a = np.where(np.abs(dd[:, 2:3]) < 0.999, np.array([[0, 0, 1.0]]), np.array([[1.0, 0, 0]]))
        u1 = np.cross(dd, a)
        u1 /= np.linalg.norm(u1, axis=1, keepdims=True)
        u2 = np.cross(dd, u1)
        d[idx] = dd * ct[:, None] + u1 * (st_ * np.cos(phi))[:, None] + u2 * (st_ * np.sin(phi))[:, None]
        keep = ~gone & (w[alive] >= 1e-4)
        # a photon in empty space far from any medium heading away is gone too (outside the box bounds above)
        alive = alive[keep]
    cat = lambda t: np.concatenate(t) if t else np.zeros((0, 3))
    return cat(tA), cat(tB), cat(tC), it


def radiance(t, c0, c1, ycut, centres, half):
    omega = 2 * math.pi * (1 - CONE)
    mu = (1 + CONE) / 2
    L, cnt = [], []
    for c in centres:
        m = (np.abs(t[:, c0] - c) <= half) & (np.abs(t[:, c1]) <= ycut)
        area = (2 * half) * (2 * ycut)
        L.append(float(t[m, 2].sum()) / NPH / (area * omega * mu))
        cnt.append(int(m.sum()))
    return L, cnt


def model(pts):
    inp = ''.join('%.9g %.9g %.9g %.9g %.9g %.9g\n' % tuple(p) for p in pts)
    out = subprocess.run([DUMP] + [str(v) for v in ("field", SIGMA, ALB, K.MSG, BASE, TOP, *SRC, 1.0)], env=ENV, input=inp,
                         capture_output=True, text=True, check=True).stdout.split('\n')
    return [float(l.split()[1]) for l in out if l.startswith('f ')]


cache = os.path.join(scratch, 'edge_mc_%d.npz' % NPH)
if os.path.exists(cache):
    z = dict(np.load(cache))
    TA, TB, TC, its = z['a'], z['b'], z['c'], int(z['its'])
else:
    TA, TB, TC, its = mc(NPH)
    np.savez(cache, a=TA, b=TB, c=TC, its=its)
print('MC %d photons, %d flights, tallies A %d B %d C %d' % (NPH, its, len(TA), len(TB), len(TC)), flush=True)


def judge(name, mcL, cnt, mod, labels):
    """every bin must hold >= 400 photons (a gate with an unjudged bin FAILS: more photons, not a vacuous pass)"""
    ok, rows = True, []
    for lab, a, c, m in zip(labels, mcL, cnt, mod):
        r = m / a if a > 0 else float('inf')
        ok &= c >= 400 and 0.5 <= r <= 2.0
        rows.append('%s: model %.4g vs MC %.4g (%d ph) ratio %.3f%s' % (lab, m, a, c, r, '' if c >= 400 else ' TOO FEW'))
    gate(name, ok, '; '.join(rows) + ' (bar 0.5 .. 2.0)')


def model_bins(centres, half, ycut, point, d):
    """the model averaged over each tally bin (5 x 5 points), as the Monte Carlo bins it"""
    out = []
    for c in centres:
        pts = [point(c + half * (i / 2.0 - 1.0) * 0.8, ycut * (j / 2.0 - 1.0) * 0.8) + d for i in range(5) for j in range(5)]
        v = model(pts)
        out.append(sum(v) / len(v))
    return out


# bins (amended after a 100k-photon look before any bar was judged: C's bins widened to x +- 0.25, |y| <= 0.5 km, and every
# model value is the bin's average, since 0.3 x 0.3 km bins held < 10 photons)
if 'A' in only:
    xs = [-1.0, -0.6, -0.3, -0.1]
    mcL, cnt = radiance(TA, 0, 1, 0.15, xs, 0.05)
    mod = model_bins(xs, 0.05, 0.15, lambda a, b: (a, b, BASE + 1.0 / SIGMA), (0.0, 0.0, 1.0))
    judge('A base', mcL, cnt, mod, ['x %.1f' % x for x in xs])
if 'B' in only:
    zs = [1.75, SRC[2], 2.6]
    mcL, cnt = radiance(TB, 1, 0, 0.15, zs, 0.05)
    mod = model_bins(zs, 0.05, 0.15, lambda a, b: (-1.0 / SIGMA, b, a), (-1.0, 0.0, 0.0))
    judge('B side', mcL, cnt, mod, ['z %.3f' % z for z in zs])
if 'C' in only:
    xs = [0.5, 1.0, 1.5]
    mcL, cnt = radiance(TC, 0, 1, 1.0, xs, 0.25)
    a = 1.0 - math.exp(-WSIG * (WZ1 - WZ0))
    mod = [a * v for v in model_bins(xs, 0.25, 1.0, lambda p, q: (p, q, 0.5 * (WZ0 + WZ1)), (0.0, 0.0, 1.0))]
    judge('C wisp', mcL, cnt, mod, ['x %.1f' % x for x in xs])
    # INFO: a single-scattering candidate, w x P(theta) / (4 pi r^2) from the strike, x the opacity, bin-averaged
    cand = []
    for c in xs:
        v = []
        for i in range(5):
            for j in range(5):
                p = np.array([c + 0.25 * (i / 2.0 - 1.0) * 0.8, 1.0 * (j / 2.0 - 1.0) * 0.8, 0.5 * (WZ0 + WZ1)])
                rv = p - np.array(SRC)
                r = float(np.linalg.norm(rv))
                ct = float(np.dot(rv / r, [0, 0, -1.0]))
                v.append(ALB * float(K.droplet(K.drop, np.array([ct]))[0]) / (4 * math.pi * r * r))
        cand.append(a * sum(v) / len(v))
    print('C cand     INFO  ' + '; '.join('x %.1f: single-scatter %.4g vs MC %.4g ratio %.3f' % (x, cv, m, cv / m if m > 0 else 0)
                                       for x, cv, m in zip(xs, cand, mcL)), flush=True)

print('LIGHTNINGEDGE-CHECK %s' % ('PASS' if results and all(results) else 'FAIL'))
if __name__ == '__main__':
    sys.exit(0 if results and all(results) else 1)
