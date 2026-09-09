"""Settle the FO4 terrain LOD _msn encoding by the only test that decides it:
a normal map must decode to UNIT VECTORS. Try every plausible decoding and
report mean |n| and its stddev per pixel. The right one gives |n| = 1.

Hypotheses:
  H1  n = (R,G,B)*2-1                      classic 0.5+0.5, up would be GREEN
  H2  n = (R*2-1, B*2-1, G*2-1)            same but swizzled so GREEN is +Z(up)
  H3  n = (R*2-1, B*2-1, G)                GREEN is +Z stored DIRECT 0..1
                                           (valid for terrain: z is never < 0)
  H4  n = (R*2-1, G*2-1, B*2-1)            up would be BLUE (tangent-space)
  H5  n = (G*2-1, R*2-1, B*2-1)            other swizzles, for completeness
  H6  n = (B*2-1, R*2-1, G)
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds

T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'


def u(v):   # 0..255 -> -1..1
    return v / 255.0 * 2.0 - 1.0


def d(v):   # 0..255 -> 0..1
    return v / 255.0


HYP = {
    'H1 (R,G,B)*2-1            ': lambda r, g, b: (u(r), u(g), u(b)),
    'H2 (R*2-1, B*2-1, G*2-1)  ': lambda r, g, b: (u(r), u(b), u(g)),
    'H3 (R*2-1, B*2-1, G direct)': lambda r, g, b: (u(r), u(b), d(g)),
    'H4 tangent up=BLUE        ': lambda r, g, b: (u(r), u(g), u(b)),
    'H5 (G*2-1, R*2-1, B*2-1)  ': lambda r, g, b: (u(g), u(r), u(b)),
    'H6 (B*2-1, R*2-1, G direct)': lambda r, g, b: (u(b), u(r), d(g)),
}


def run(path, mip=2, step=1):
    dd = dds.DDS(path)
    mw, mh, px = dd.decode(mip)
    acc = {k: [0.0, 0.0, 0] for k in HYP}
    for i in range(0, mw * mh, step):
        r, g, b = px[i * 4], px[i * 4 + 1], px[i * 4 + 2]
        for k, f in HYP.items():
            n = f(r, g, b)
            L = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
            a = acc[k]
            a[0] += L; a[1] += L * L; a[2] += 1
    out = {}
    for k, (s, s2, n) in acc.items():
        m = s / n
        out[k] = (m, max(0.0, s2 / n - m * m) ** 0.5)
    return mw, mh, out


if __name__ == '__main__':
    names = sys.argv[1:] or ['Commonwealth.4.-20.24_msn', 'Commonwealth.4.-20.60_msn',
                             'Commonwealth.4.-60.60_msn', 'Commonwealth.4.0.0_msn',
                             'Commonwealth.32.-96.-96_msn', 'Commonwealth.16.-16.0_msn']
    for nm in names:
        p = nm if os.path.isabs(nm) else os.path.join(T, nm + '.DDS')
        mw, mh, out = run(p, mip=2)
        print('%s  (mip2 %dx%d)' % (os.path.basename(p), mw, mh))
        for k in sorted(out):
            m, sd = out[k]
            flag = '   <== UNIT' if abs(m - 1.0) < 0.02 and sd < 0.05 else ''
            print('    %s  |n| mean %.4f  std %.4f%s' % (k, m, sd, flag))
        print()
