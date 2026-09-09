"""What convention is FO4 terrain LOD _msn in? Measure all four channels."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds
T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'

def chan(path, mip=0):
    d = dds.DDS(path)
    mw, mh, px = d.decode(mip)
    n = mw * mh
    s = [0] * 4
    mn = [255] * 4
    mx = [0] * 4
    sq = [0] * 4
    for i in range(n):
        for c in range(4):
            v = px[i * 4 + c]
            s[c] += v; sq[c] += v * v
            if v < mn[c]: mn[c] = v
            if v > mx[c]: mx[c] = v
    mean = [x / n for x in s]
    std = [max(0.0, sq[c] / n - mean[c] ** 2) ** 0.5 for c in range(4)]
    return mw, mh, d.fmt, mean, std, mn, mx

if __name__ == '__main__':
    args = sys.argv[1:]
    if not args:
        args = ['Commonwealth.4.-20.24_msn', 'Commonwealth.4.-20.60_msn',
                'Commonwealth.4.-20.80_msn', 'Commonwealth.4.0.0_msn',
                'Commonwealth.4.-60.60_msn', 'Commonwealth.32.-96.-96_msn',
                'Commonwealth.16.-16.0_msn', 'Commonwealth.8.-24.24_msn']
    for a in args:
        p = a if os.path.isabs(a) else os.path.join(T, a + '.DDS')
        mw, mh, fmt, mean, std, mn, mx = chan(p)
        print('%-30s %s %dx%d' % (os.path.basename(p).replace('.DDS', ''), fmt, mw, mh))
        for c, nm in enumerate('RGBA'):
            print('    %s mean %6.1f  std %5.1f  min %3d  max %3d' % (nm, mean[c], std[c], mn[c], mx[c]))
