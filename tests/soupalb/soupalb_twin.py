"""FIX1031B SOUPALB1 twin: covered shade vs open shade in a probe's up-facing irradiance.

A porch (house wall x = 0 facing +x, roof slab z = H over 0 <= x <= D), infinite ground, a uniform sky of
irradiance 1 and a sun of normal irradiance ES. The probes store what the ray-traced GI volume stores: the
cosine-weighted irradiance of everything the probe's rays hit (sky misses + shaded hits with the sun at the hit +
further bounces), never the direct sun at the probe itself. Path tracer, numpy, fixed seed.

  python -I soupalb_twin.py [grey]     # grey = today's live soup (every surface albedo 0.5, the red)
Prints `covered/open R` and `SOUPALB TWIN OK|FAIL` against the rule covered < open (R < BAR).
"""
import sys
import numpy as np

H, D, WALLTOP, LEN = 300.0, 300.0, 600.0, 4000.0   # porch roof height, depth; wall top; half length along y
# The census runs' CommonwealthClear (notes/fix1031b/gi/cli1.sh --sun / --sky): sun rgb 3.530 3.053 2.416 at
# elevation 33.5 deg, sky +Z irradiance 0.132 0.239 0.398 -> luminance 3.108 / 0.228 = 13.6 x the sky.
ES = 13.6
SUN = np.array([0.83352, 0.0, 0.55249]); SUN /= np.linalg.norm(SUN)   # in front of the porch: the open ground is lit
BAR = 1.0
N = 120000
DEPTH = 3
# linear albedo: the bake's own stated ground fallback (cellview.cpp: sRGB 110, 100, 85 -> linear, grey mean)
def s2l(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
DIRT = float(np.mean([s2l(110), s2l(100), s2l(85)]))
WOOD = DIRT * 1.4    # a weathered painted wood underside, a little lighter than dirt (REASONED)


def albedos(grey):
    if grey:
        return 0.5, 0.5, 0.5
    return DIRT, WOOD, WOOD   # ground, roof, wall


def intersect(o, d):
    """t and kind (0 none, 1 ground, 2 roof, 3 wall) and normal facing the ray."""
    n = o.shape[0]
    t = np.full(n, np.inf); kind = np.zeros(n, np.int8); nz = np.zeros((n, 3))
    eps = 1e-3
    # ground z = 0
    with np.errstate(divide='ignore', invalid='ignore'):
        tg = np.where(d[:, 2] < 0, -o[:, 2] / d[:, 2], np.inf)
    m = (tg > eps) & (tg < t); t[m] = tg[m]; kind[m] = 1
    # roof z = H, 0 <= x <= D, |y| <= LEN
    with np.errstate(divide='ignore', invalid='ignore'):
        tr = (H - o[:, 2]) / d[:, 2]
    p = o + d * tr[:, None]
    m = (tr > eps) & (tr < t) & (p[:, 0] >= 0) & (p[:, 0] <= D) & (np.abs(p[:, 1]) <= LEN)
    t[m] = tr[m]; kind[m] = 2
    # wall x = 0, 0 <= z <= WALLTOP, |y| <= LEN (the house; behind it is solid)
    with np.errstate(divide='ignore', invalid='ignore'):
        tw = -o[:, 0] / d[:, 0]
    p = o + d * tw[:, None]
    m = (tw > eps) & (tw < t) & (p[:, 2] >= 0) & (p[:, 2] <= WALLTOP) & (np.abs(p[:, 1]) <= LEN)
    t[m] = tw[m]; kind[m] = 3
    nz[kind == 1] = [0, 0, 1]
    nz[kind == 2] = [0, 0, -1]
    nz[kind == 3] = [1, 0, 0]
    flip = np.einsum('ij,ij->i', nz, d) > 0
    nz[flip] *= -1
    return t, kind, nz


def cosdir(nrm, rng):
    n = nrm.shape[0]
    u1, u2 = rng.random(n), rng.random(n)
    r, ph = np.sqrt(u1), 2 * np.pi * u2
    loc = np.stack([r * np.cos(ph), r * np.sin(ph), np.sqrt(1 - u1)], 1)
    a = np.where(np.abs(nrm[:, 0:1]) > 0.9, [[0, 1, 0]], [[1, 0, 0]])
    tx = np.cross(nrm, a); tx /= np.linalg.norm(tx, axis=1)[:, None]
    ty = np.cross(nrm, tx)
    return loc[:, 0:1] * tx + loc[:, 1:2] * ty + loc[:, 2:3] * nrm


def radiance(o, d, depth, alb, rng):
    t, kind, nrm = intersect(o, d)
    L = np.zeros(o.shape[0])
    miss = kind == 0
    L[miss] = np.where(d[miss, 2] > 0, 1.0 / np.pi, 0.0)   # sky radiance: irradiance 1 on an open up face
    hit = ~miss
    if not hit.any():
        return L
    p = o[hit] + d[hit] * t[hit, None] + nrm[hit] * 1e-2
    a = np.choose(kind[hit] - 1, alb)
    cs = np.clip(nrm[hit] @ SUN, 0, None)
    _, sk, _ = intersect(p, np.repeat(SUN[None], p.shape[0], 0))
    direct = a / np.pi * ES * cs * (sk == 0)
    ind = np.zeros(p.shape[0])
    if depth > 0:
        ind = a * radiance(p, cosdir(nrm[hit], rng), depth - 1, alb, rng)
    L[hit] = direct + ind
    return L


def up_irradiance(pos, alb, seed):
    rng = np.random.default_rng(seed)
    o = np.repeat(np.array(pos, float)[None], N, 0)
    d = cosdir(np.repeat(np.array([[0.0, 0.0, 1.0]]), N, 0), rng)
    return np.pi * radiance(o, d, DEPTH, alb, rng).mean()


def main():
    grey = len(sys.argv) > 1 and sys.argv[1] == 'grey'
    alb = albedos(grey)
    cov = up_irradiance((D * 0.5, 0.0, 120.0), alb, 1)
    opn = up_irradiance((D + 3000.0, 0.0, 120.0), alb, 2)
    r = cov / opn
    print('albedo ground/roof/wall %.3f %.3f %.3f (%s)' % (alb + ('grey: today\'s live soup' if grey else 'per surface',)))
    print('up irradiance covered %.4f open %.4f covered/open %.4f bar < %.2f' % (cov, opn, r, BAR))
    ok = r < BAR
    print('SOUPALB TWIN ' + ('OK' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
