# lane WEATHERBLEND1: the hand values of weatherblend.cpp, computed apart from the C++ (float64, the engine's CIELab
# constants written out again). usage: python -I expected.py
import math


def to_lab(rgb):
    c = []
    for v in rgb:
        v = v / 255.0
        c.append((((v + 0.055) / 1.055) ** 2.4 if v > 0.04045 else v / 12.92) * 100.0)
    X = (c[0] * 0.4124 + c[1] * 0.3576 + c[2] * 0.1805) / 95.047
    Y = (c[0] * 0.2126 + c[1] * 0.7152 + c[2] * 0.0722) / 100.0
    Z = (c[0] * 0.0193 + c[1] * 0.1192 + c[2] * 0.9505) / 108.883
    f = lambda t: t ** (1.0 / 3.0) if t > 0.008856 else t * 7.787 + 16.0 / 116.0
    fx, fy, fz = f(X), f(Y), f(Z)
    return [116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)]


def from_lab(l):
    fy = (l[0] + 16.0) / 116.0
    fx = l[1] / 500.0 + fy
    fz = fy - l[2] / 200.0
    g = lambda v: v ** 3 if v ** 3 > 0.008856 else (v - 16.0 / 116.0) / 7.787
    X, Y, Z = g(fx) * 0.95047, g(fy), g(fz) * 1.08883
    c = [X * 3.2406 - Y * 1.5372 - Z * 0.4986, -X * 0.9689 + Y * 1.8758 + Z * 0.0415, X * 0.0557 - Y * 0.2040 + Z * 1.0570]
    out = []
    for v in c:
        e = 1.055 * v ** (1 / 2.4) - 0.055 if v > 0.0031308 else v * 12.92
        out.append(min(max(e, 0.0), 1.0) * 255.0)
    return out


def blend4(cols, wt):
    l = [0.0, 0.0, 0.0]
    for c, w in zip(cols, wt):
        if w:
            lab = to_lab(c)
            l = [a + b * w for a, b in zip(l, lab)]
    return from_lab(l)


def weights(t, p):
    return [(1 - t) * p, t * p, (1 - t) * (1 - p), t * (1 - p)]


NEW_DAY, NEW_SUNSET = (20, 60, 220), (240, 120, 10)
OLD_DAY, OLD_SUNSET = (200, 100, 50), (90, 200, 160)
print('one key (Day), new', NEW_DAY, 'old', OLD_DAY)
for p in (0.0, 0.5, 1.0):
    print('  pct %.1f' % p, ['%.3f' % v for v in blend4([NEW_DAY, NEW_DAY, OLD_DAY, OLD_DAY], weights(0.0, p))])
print('two keys Day->Sunset t=0.25')
for p in (0.0, 0.5, 1.0):
    print('  pct %.1f' % p, ['%.3f' % v for v in blend4([NEW_DAY, NEW_SUNSET, OLD_DAY, OLD_SUNSET], weights(0.25, p))])
print('cloud: a zero new colour takes the old one; old', OLD_DAY, 'new (0,0,0), pct 0.5:',
      ['%.3f' % v for v in blend4([OLD_DAY, OLD_DAY, OLD_DAY, OLD_DAY], weights(0.0, 0.5))])
# the change length: Trans Delta / 255 x (Max - Min) + Min game hours, x 3600 / 20 wall seconds (Fallout4.esm: Max 3)
for d in (0, 128, 255):
    h = d / 255.0 * (3.0 - 0.01) + 0.01
    print('delta %3d: %.6f game hours, %.4f s; pct at half = 0.5 at %.4f s' % (d, h, h * 180.0, h * 90.0))

# lane WEATHERBLEND2: the imagespace slots (cur a, cur b, last a, last b), t 0.25 both, pct 0.3 and 0.7
for p in (0.3, 0.7):
    print('slots pct %.1f' % p, ['%.4f' % v for v in weights(0.25, p)])
# four LUT texels by the pct 0.3 weights; then without the third LUT (renormalised over the other three)
LUTS = [(0, 100, 255), (255, 0, 10), (40, 40, 40), (200, 220, 0)]
w = weights(0.25, 0.3)
print('lut4', [round(sum(l[i] * x for l, x in zip(LUTS, w))) for i in range(3)])
w3 = [w[0], w[1], 0.0, w[3]]
print('lut3', [round(sum(l[i] * x for l, x in zip(LUTS, w3)) / sum(w3)) for i in range(3)])
# fWeatherTransAccel 16 (Fallout4.esm): accelerated at pct s, pct = s + (raw - s) x 17
for s, raw in ((0.2, 0.21), (0.2, 0.25), (0.5, 0.5)):
    print('accel s %.2f raw %.2f -> %.4f' % (s, raw, min(1.0, s + (raw - s) * 17.0)))
# delta 128: accelerated at half way (135.9776 s) it ends when raw = 0.5 + 0.5 / 17
h = 128 / 255.0 * (3.0 - 0.01) + 0.01
print('delta 128 accelerated at 0.5 ends at %.4f s (unaccelerated %.4f s)' % ((0.5 + 0.5 / 17.0) * h * 180.0, h * 180.0))


# lane WEATHERBLEND3: the pct integral by the midpoint rule (delta 255, 1..3 h: 540 s), a wait shifts the start
def pct_at(u, cE, f):
    raw = min(u / cE, 1.0)
    if f < 0 or raw <= f:
        return raw
    return min(1.0, f + (raw - f) * 17.0)


def integral(secs, wait_h, f, n=200000):
    cE, S = 540.0, wait_h * 180.0
    d = secs / n
    return sum(pct_at(S + (k + 0.5) * d, cE, f) for k in range(n)) * d


for secs, wait_h, f in ((270.0, 0.0, -1.0), (600.0, 0.0, -1.0), (180.0, 1.0, -1.0), (280.0, 0.0, 0.5)):
    print('pct integral %.0f s wait %.0f h accel %.1f = %.4f' % (secs, wait_h, f, integral(secs, wait_h, f)))
# the one scroll: 0.25 + 0.5 x 0.1 x 10 + (1.5 - 0.5) x 0.1 x 4, wrapped; wind 200 / 51 at 30%
print('one scroll %.4f' % ((0.25 + 0.05 * 10 + 0.1 * 4) % 1.0), 'wind %.4f' % (200 * 0.3 + 51 * 0.7))
