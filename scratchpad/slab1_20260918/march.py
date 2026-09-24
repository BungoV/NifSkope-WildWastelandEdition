# Lane SLAB1 -- THE LAW, REIMPLEMENTED OUTSIDE THE EXE.
#
# Both marches, written from the C++ a second time in Python, run over the
# lattice the exe itself dumped (probe/objh_after.bin, v2: MAX then MIN) with
# h0 taken from the ESM LAND heights the exe read (probe/land.bin, bilinear).
#
# The point is not to compute the mask -- the exe already did that -- but to
# PREDICT the direction and the size of the move of each rectangle from the
# geometry alone, so "why did the pier foot brighten" is answered with a ratio
# and not with a story.  If this reimplementation and the shipped exe disagree
# on which rectangles rise, one of the two is wrong and the disagreement is the
# finding.
import math, os, sys
from clearance import read_objh, Land, SENT

S = os.path.dirname(os.path.abspath(__file__))
CELL = 128.0
DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1),
        (0.7071, 0.7071), (0.7071, -0.7071), (-0.7071, 0.7071), (-0.7071, -0.7071)]
STEPS = []
d = 128.0
while d <= 2048.0:
    STEPS.append(d)
    d *= 1.5


class Field:
    def __init__(self, o):
        self.o = o

    def span(self, wx, wy):
        o = self.o
        gx = int(math.floor(wx / CELL)) - o['gx0']
        gy = int(math.floor(wy / CELL)) - o['gy0']
        if gx < 0 or gy < 0 or gx >= o['gw'] or gy >= o['gh']:
            return (None, None)
        k = gy * o['gw'] + gx
        hi = o['mx'][k]
        if hi < SENT:
            return (None, None)
        return (o['mn'][k], hi)


def sky_vis(f, wx, wy, h0, strength, slab):
    if strength <= 0.0:
        return 1.0
    occl = 0.0
    for dx, dy in DIRS:
        wall = 0.0
        ceil_open = 0.0
        have_ceil = False
        covered = True
        for dist in STEPS:
            lo, hi = f.span(wx + dx * dist, wy + dy * dist)
            if hi is None:
                covered = False
                continue
            if (not slab) or lo <= h0:
                covered = False
                dh = hi - h0
                if dh > 0.0:
                    wall = max(wall, dh / dist)
            elif covered:
                op = (lo - h0) / dist
                if (not have_ceil) or op < ceil_open:
                    ceil_open = op
                    have_ceil = True
        wb = wall / (1.0 + wall)
        if not have_ceil:
            occl += wb
        else:
            occl += min(1.0, wb + (1.0 - ceil_open / (1.0 + ceil_open)))
    if occl == 0.0:
        return 1.0
    return min(1.0, max(0.0, 1.0 - occl / 8.0 * 1.6 * strength))


def terrain_at(land, wx, wy):
    """Bilinear ESM LAND height at a world point, game units (8-unit quantised)."""
    gx = wx / CELL
    gy = wy / CELL
    ix = int(math.floor(gx)); iy = int(math.floor(gy))
    fx = gx - ix; fy = gy - iy
    def n(gxx, gyy):
        cx, sx = divmod(gxx, 32)
        cy, sy = divmod(gyy, 32)
        return land.node(cx, cy, sx, sy)
    a = n(ix, iy); b = n(ix + 1, iy); c = n(ix, iy + 1); dd = n(ix + 1, iy + 1)
    if None in (a, b, c, dd):
        return None
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + dd * fx) * fy


def rect(f, land, name, x0, y0, x1, y1, step=32.0, strength=0.5):
    so = sn = 0.0
    k = 0
    y = y0 + step * 0.5
    while y < y1:
        x = x0 + step * 0.5
        while x < x1:
            h0 = terrain_at(land, x, y)
            if h0 is not None:
                so += sky_vis(f, x, y, h0, strength, False)
                sn += sky_vis(f, x, y, h0, strength, True)
                k += 1
            x += step
        y += step
    if not k:
        print('%-30s NO SAMPLES' % name)
        return
    print('%-30s samples %6d   old vis %.4f   new vis %.4f   ratio %.4f' %
          (name, k, so / k, sn / k, (sn / k) / (so / k) if so else float('nan')))


def main():
    o = read_objh(os.path.join(S, 'probe', 'objh_after.bin'))
    assert o['two'], 'need the v2 dump with the min plane'
    land = Land(os.path.join(S, 'probe', 'land.bin'))
    f = Field(o)
    print('steps', ' '.join('%.0f' % s for s in STEPS))
    R = [
        ('a-deck (elevated highway)', 19712, -41856, 20992, -40576),
        ('a2-brief (the brief coord)', 24644, -41556, 25156, -41044),
        ('a3-catwalk', 31616, -41472, 32640, -40576),
        ('c-open (control)', 23936, -34816, 24192, -34560),
        ('d-pierfoot', 31232, -45568, 31616, -44160),
    ]
    for nm, x0, y0, x1, y1 in R:
        rect(f, land, nm, x0, y0, x1, y1)
    # the whole chunk, coarser
    rect(f, land, 'whole chunk 4.4.-12', 16384, -49152, 32768, -32768, step=128.0)


if __name__ == '__main__':
    main()
