"""What does a GREEN/BLUE swap in the terrain LOD _msn actually cost?

Vanilla FO4 stores the model-space terrain normal as R = X(east), G = UP,
B = Y(north), all 0.5+0.5 encoded (settled by msn_updecide.py). A tool that
writes the natural (X, Y, Z) order instead puts UP in BLUE -- which is what
src/lodgen.cpp:5021-5027 in this repo does.

Simulate the shading the game computes both ways, on real vanilla data:
  correct:  n = (R, G, B) read as (x, up, y)     <- what the shader expects
  swapped:  the same surface written (x, y, up), then read as (x, up, y)
For each, report mean and stddev of the diffuse term max(0, n.L) under a sun,
and the same under a sky-ambient-plus-sun model. Darker = lower mean.
Flatter = lower stddev.
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds

T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'


def stats(v):
    n = len(v)
    m = sum(v) / n
    s = math.sqrt(max(0.0, sum(x * x for x in v) / n - m * m))
    return m, s


def run(tile, mip=2, sunAz=225.0, sunEl=45.0, ambient=0.25):
    d = dds.DDS(os.path.join(T, tile + '_msn.DDS'))
    w, h, px = d.decode(mip)
    a = math.radians(sunAz); e = math.radians(sunEl)
    L = (math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e))
    good = []; bad = []
    for i in range(w * h):
        R, G, B = px[i * 4], px[i * 4 + 1], px[i * 4 + 2]
        # decode the true surface the way the game does: x=R, up=G, y=B
        x = R / 255.0 * 2 - 1
        up = G / 255.0 * 2 - 1
        y = B / 255.0 * 2 - 1
        # correct: shader gets (x, y, up)
        nGood = (x, y, up)
        # swapped writer: it emitted R=x, G=y, B=up for THIS SAME surface.
        # the shader still reads x=R, up=G, y=B, so it reconstructs:
        nBad = (x, up, y)          # (east, north<-up, up<-north)
        for n, out in ((nGood, good), (nBad, bad)):
            ln = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
            nl = max(0.0, (n[0] * L[0] + n[1] * L[1] + n[2] * L[2]) / ln)
            out.append(ambient + (1.0 - ambient) * nl)
    return w * h, stats(good), stats(bad)


if __name__ == '__main__':
    tiles = sys.argv[1:] or ['Commonwealth.4.-20.24', 'Commonwealth.4.-20.60',
                             'Commonwealth.4.-60.60', 'Commonwealth.4.0.0',
                             'Commonwealth.4.-20.40', 'Commonwealth.16.-16.0',
                             'Commonwealth.32.-96.-96']
    print('sun az 225 el 45, ambient 0.25; light = 0.25 + 0.75*max(0, n.L)')
    print('%-24s %-22s %-22s  %s' % ('tile', 'CORRECT mean/std', 'SWAPPED mean/std', 'delta'))
    gm = bm = gs = bs = 0.0; k = 0
    for t in tiles:
        n, (g_m, g_s), (b_m, b_s) = run(t)
        print('%-24s  %.3f / %.3f          %.3f / %.3f         light %+.1f%%  variation %+.1f%%'
              % (t.replace('Commonwealth.', ''), g_m, g_s, b_m, b_s,
                 100 * (b_m - g_m) / g_m, 100 * (b_s - g_s) / g_s))
        gm += g_m; bm += b_m; gs += g_s; bs += b_s; k += 1
    print('%-24s  %.3f / %.3f          %.3f / %.3f         light %+.1f%%  variation %+.1f%%'
          % ('MEAN', gm / k, gs / k, bm / k, bs / k,
             100 * (bm - gm) / gm, 100 * (bs - gs) / gs))
