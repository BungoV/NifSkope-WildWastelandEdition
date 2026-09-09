"""WHICH CHANNEL IS UP? A magnitude test cannot tell -- permutations of the same
three components have the same length, so H1/H2/H4/H5 in msn_convention.py are
algebraically identical and that test was worthless for the swizzle. This one
is not.

For a terrain normal the UP component is not free: it is determined by the other
two, z = +sqrt(1 - x^2 - y^2), and it is always POSITIVE. The two horizontal
components are free and signed. So:

  TEST 1 (determinism): for each candidate "up" channel c, predict it from the
  other two as 255*(0.5 + 0.5*sqrt(max(0, 1 - a^2 - b^2))) and measure the
  residual. The true up channel has a small residual; a horizontal channel does
  not, because its sign is not recoverable.

  TEST 2 (one-sidedness): the up channel must never encode a negative value,
  i.e. never drop below 128 by more than compression error. Report the fraction
  of pixels below 128 for each channel.
"""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dds

T = r'E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth'


def run(path, mip=2):
    dd = dds.DDS(path)
    mw, mh, px = dd.decode(mip)
    n = mw * mh
    # residual of predicting each channel from the other two
    res = [0.0, 0.0, 0.0]
    below = [0, 0, 0]
    for i in range(n):
        c = [px[i * 4], px[i * 4 + 1], px[i * 4 + 2]]
        v = [x / 255.0 * 2.0 - 1.0 for x in c]
        for k in range(3):
            a, b = v[(k + 1) % 3], v[(k + 2) % 3]
            pred = math.sqrt(max(0.0, 1.0 - a * a - b * b))
            predByte = (pred * 0.5 + 0.5) * 255.0
            res[k] += abs(predByte - c[k])
            if c[k] < 128:
                below[k] += 1
    return mw, mh, [r / n for r in res], [b / n for b in below]


if __name__ == '__main__':
    names = sys.argv[1:] or [
        'Commonwealth.4.-20.24_msn', 'Commonwealth.4.-20.60_msn',
        'Commonwealth.4.-60.60_msn', 'Commonwealth.4.0.0_msn',
        'Commonwealth.4.-20.80_msn', 'Commonwealth.8.-24.24_msn',
        'Commonwealth.16.-16.0_msn', 'Commonwealth.32.-96.-96_msn']
    print('%-30s | mean |predicted-actual| (bytes) | fraction of pixels < 128' % 'tile')
    print('%-30s |    R      G      B    |    R      G      B' % '')
    for nm in names:
        p = nm if os.path.isabs(nm) else os.path.join(T, nm + '.DDS')
        mw, mh, res, below = run(p, mip=2)
        best = min(range(3), key=lambda k: res[k])
        print('%-30s | %6.1f %6.1f %6.1f | %5.3f %5.3f %5.3f   -> up = %s'
              % (os.path.basename(p).replace('.DDS', ''), res[0], res[1], res[2],
                 below[0], below[1], below[2], 'RGB'[best]))
