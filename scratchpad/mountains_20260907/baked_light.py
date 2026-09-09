"""Is vanilla's terrain LOD DIFFUSE plain albedo, or does it have light baked in?

This decides whether the _msn matters at all. If Bethesda baked a directional
sun term into the diffuse, a rebake that ships flat albedo loses the shading
regardless of normals. If the diffuse is plain albedo, then ALL the shading at
distance comes from the _msn, and a wrong _msn is catastrophic.

Method: pair each texel of the diffuse with the same texel of the _msn (both are
512x512, same footprint). Then measure
  (1) correlation of diffuse luminance with the msn's UP component (GREEN)
      -- a proxy for slope/AO baked in;
  (2) the best directional fit: for a grid of sun azimuths, correlation of
      diffuse luminance with n.L. If some direction correlates strongly and the
      correlation peaks at one azimuth, a sun was baked in.
Also reports what the diffuse's ALPHA channel carries.
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds

T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'
# vanilla msn convention, settled by msn_updecide.py: R = X, G = UP, B = Y
def normal(r, g, b):
    return (r / 255.0 * 2 - 1, b / 255.0 * 2 - 1, g / 255.0 * 2 - 1)  # (x, y, up)


def corr(xs, ys):
    n = len(xs)
    mx = sum(xs) / n; my = sum(ys) / n
    sxy = sxx = syy = 0.0
    for i in range(n):
        a = xs[i] - mx; b = ys[i] - my
        sxy += a * b; sxx += a * a; syy += b * b
    if sxx <= 0 or syy <= 0:
        return 0.0
    return sxy / math.sqrt(sxx * syy)


def run(tile, mip=2):
    dif = dds.DDS(os.path.join(T, tile + '.DDS'))
    nrm = dds.DDS(os.path.join(T, tile + '_msn.DDS'))
    dw, dh, dp = dif.decode(mip)
    nw, nh, npx = nrm.decode(mip)
    assert (dw, dh) == (nw, nh), 'size mismatch %s vs %s' % ((dw, dh), (nw, nh))
    lum = []; ups = []; ns = []
    aMin, aMax, aSum = 255, 0, 0
    for i in range(dw * dh):
        r, g, b, a = dp[i * 4], dp[i * 4 + 1], dp[i * 4 + 2], dp[i * 4 + 3]
        lum.append(0.2126 * r + 0.7152 * g + 0.0722 * b)
        aSum += a; aMin = min(aMin, a); aMax = max(aMax, a)
        n = normal(npx[i * 4], npx[i * 4 + 1], npx[i * 4 + 2])
        ns.append(n); ups.append(n[2])
    out = {'tile': tile, 'n': dw * dh,
           'alpha': (aSum / (dw * dh), aMin, aMax),
           'corr_up': corr(lum, ups)}
    best = (0.0, None)
    for az in range(0, 360, 15):
        for el in (20, 40, 60):
            a = math.radians(az); e = math.radians(el)
            L = (math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e))
            nl = [max(0.0, n[0] * L[0] + n[1] * L[1] + n[2] * L[2]) for n in ns]
            c = corr(lum, nl)
            if abs(c) > abs(best[0]):
                best = (c, (az, el))
    out['best_dir'] = best
    return out


if __name__ == '__main__':
    tiles = sys.argv[1:] or ['Commonwealth.4.-20.24', 'Commonwealth.4.-20.60',
                             'Commonwealth.4.-60.60', 'Commonwealth.4.0.0',
                             'Commonwealth.4.-20.40', 'Commonwealth.16.-16.0']
    for t in tiles:
        o = run(t)
        print('%-26s n=%-6d alpha mean %5.1f min %3d max %3d | corr(lum, up) %+.3f | best n.L corr %+.3f at az %s'
              % (t.replace('Commonwealth.', ''), o['n'], o['alpha'][0], o['alpha'][1],
                 o['alpha'][2], o['corr_up'], o['best_dir'][0], o['best_dir'][1]))
