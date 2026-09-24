# Lane SLAB1 -- THE POPULATION REFUTER.
#
# One rectangle is an anecdote. This classifies EVERY 128-unit square of the
# baked chunk by what the march reads around it, and then compares the mask B
# mean of that square between the two bakes:
#
#   WALL-ONLY squares  -- every one of the 8 x 7 squares the march visits is a
#                         wall under the march's own test (min <= h0), so the
#                         ceiling branch is never entered and the new law is
#                         ALGEBRAICALLY the old one. These must not move. This
#                         is the refuter that the ceiling did not become
#                         "ignore objects": if the sign of the change leaked
#                         anywhere it would leak here.
#   CEILING-FED squares -- at least one visited square is a ceiling. These are
#                         the only squares allowed to move at all.
#
# Everything is in WORLD units. B is the BC1 blue channel of the role-5 mask
# sheet, so a square mean is an average of 5-bit values, not of bytes.
import math, os, sys
from clearance import read_objh, Land, SENT
import mask_b_mean as M

S = os.path.dirname(os.path.abspath(__file__))
CELL = 128.0
DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1),
        (0.7071, 0.7071), (0.7071, -0.7071), (-0.7071, 0.7071), (-0.7071, -0.7071)]
STEPS = []
_d = 128.0
while _d <= 2048.0:
    STEPS.append(_d); _d *= 1.5


def classify(o, land):
    """square (gx,gy) -> 'wall' | 'ceil' | None (nothing visible at all)."""
    out = {}
    gx0, gy0, gw, gh = o['gx0'], o['gy0'], o['gw'], o['gh']
    mx, mn = o['mx'], o['mn']
    for gy in range(-49152 // 128, -32768 // 128):
        for gx in range(16384 // 128, 32768 // 128):
            # CONSERVATIVE, because a square holds 16x16 mask texels and each
            # marches from ITS OWN position with ITS OWN h0:
            #  * the visited set is the UNION over a 4x4 grid of sample points
            #    inside the square, not the square centre alone (the centre
            #    alone misclassifies every square whose texels straddle a
            #    lattice boundary, which is all of them);
            #  * h0 is the LOWEST of the square's four LAND nodes, so 'wall'
            #    means wall for every texel in the square, not just the mean one.
            hs = land.corners(gx, gy)
            if hs is None:
                continue
            h0 = min(hs)
            seen = False
            ceil = False
            for oy in (16.0, 48.0, 80.0, 112.0):
                for ox in (16.0, 48.0, 80.0, 112.0):
                    cx = gx * CELL + ox
                    cy = gy * CELL + oy
                    for dx, dy in DIRS:
                        for d in STEPS:
                            sx = int(math.floor((cx + dx * d) / CELL)) - gx0
                            sy = int(math.floor((cy + dy * d) / CELL)) - gy0
                            if not (0 <= sx < gw and 0 <= sy < gh):
                                continue
                            k = sy * gw + sx
                            if mx[k] < SENT:
                                continue
                            seen = True
                            if mn[k] > h0:
                                ceil = True
            if not seen:
                out[(gx, gy)] = None
            else:
                out[(gx, gy)] = 'ceil' if ceil else 'wall'
    return out


def main():
    o = read_objh(os.path.join(S, 'probe', 'objh_after.bin'))
    land = Land(os.path.join(S, 'probe', 'land.bin'))
    cls = classify(o, land)
    a = M.Sheet(os.path.join(S, 'before', 'vt', 'FO4CSLOD', 'Commonwealth',
                             'Commonwealth.VT.1.lodt')).square_means(CELL)
    b = M.Sheet(os.path.join(S, 'after', 'vt', 'FO4CSLOD', 'Commonwealth',
                             'Commonwealth.VT.1.lodt')).square_means(CELL)
    groups = {'wall': [], 'ceil': [], None: []}
    for key, kind in cls.items():
        if key not in a or key not in b:
            continue
        groups[kind].append((b[key] - a[key], key))
    for kind, label in ((None, 'NOTHING VISIBLE (open sky in all 56 samples)'),
                        ('wall', 'WALL-ONLY (the ceiling branch is never entered)'),
                        ('ceil', 'CEILING-FED (at least one ceiling in the 56)')):
        g = groups[kind]
        if not g:
            print('%-46s  n 0' % label)
            continue
        ds = [d for d, _ in g]
        worst = max(g, key=lambda t: abs(t[0]))
        print('%-46s  n %5d  mean move %+7.3f  max |move| %6.3f at world (%d,%d)  '
              'moved-at-all %d' %
              (label, len(g), sum(ds) / len(ds), abs(worst[0]),
               worst[1][0] * 128, worst[1][1] * 128,
               sum(1 for d in ds if abs(d) > 1e-9)))
    print()
    print('The refuter: WALL-ONLY squares are the old law by algebra, so any '
          'nonzero move there is a defect, not a result.')


if __name__ == '__main__':
    main()
