# Lane SLAB1 -- CLEARANCE: the wall/ceiling classification of every lattice
# square in a rectangle, from two files and no guessing.
#
#   probe/objh_after.bin  the v2 lattice dump: MAX plane then MIN plane
#   probe/land.bin        `lodgen --dump-land`: the ESM LAND heights of the
#                         whole worldspace, qint16 of (height / 8) game units
#
# clearance = minZ - terrain.  The march reads a square as a WALL when
# minZ <= h0 (the sample's own terrain) and as a CEILING when the whole span
# stands above it, so clearance is the number that decides which branch a
# square takes, and 432 = sqrt(128 * 1458) is where the two laws cross.
#
# UNITS: game (world) units throughout.  land.bin stores height/8 as an
# integer, so terrain here is quantised to 8 units -- a rounding of 8 against
# clearances of hundreds, and it is stated rather than hidden.
import struct, sys, os

S = os.path.dirname(os.path.abspath(__file__))

def read_objh(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'OBJH', b[:4]
    gx0, gy0, gw, gh = struct.unpack_from('<4i', b, 4)
    cell, = struct.unpack_from('<f', b, 20)
    n = gw * gh
    assert len(b) in (24 + n * 4, 24 + n * 8), (len(b), n)
    two = len(b) == 24 + n * 8
    mx = struct.unpack_from('<%df' % n, b, 24)
    mn = struct.unpack_from('<%df' % n, b, 24 + n * 4) if two else None
    return dict(gx0=gx0, gy0=gy0, gw=gw, gh=gh, cell=cell, mx=mx, mn=mn, two=two)

class Land:
    def __init__(self, path):
        b = open(path, 'rb').read()
        self.mnx, self.mny, self.cw, self.ch = struct.unpack_from('<4i', b, 0)
        base = 16
        self.present = b[base:base + self.cw * self.ch]
        self.grid = b[base + self.cw * self.ch:]
        assert len(self.grid) == self.cw * self.ch * 33 * 33 * 2, len(self.grid)

    def node(self, cx, cy, col, row):
        """LAND height at cell (cx,cy) node (col=x, row=y), SW origin, game units."""
        if not (self.mnx <= cx < self.mnx + self.cw and self.mny <= cy < self.mny + self.ch):
            return None
        ci = (cy - self.mny) * self.cw + (cx - self.mnx)
        if not self.present[ci]:
            return None
        o = (ci * 33 * 33 + row * 33 + col) * 2
        v, = struct.unpack_from('<h', self.grid, o)
        return v * 8.0

    def corners(self, gx, gy):
        """The four LAND nodes of lattice square (gx,gy), game units, or None."""
        cx, sx = divmod(gx, 32)
        cy, sy = divmod(gy, 32)
        vs = [self.node(cx, cy, sx, sy), self.node(cx, cy, sx + 1, sy),
              self.node(cx, cy, sx, sy + 1), self.node(cx, cy, sx + 1, sy + 1)]
        return None if any(v is None for v in vs) else vs

    def square(self, gx, gy):
        """Terrain under lattice square (gx,gy): the mean of its four nodes."""
        cx, sx = divmod(gx, 32)
        cy, sy = divmod(gy, 32)
        vs = [self.node(cx, cy, sx, sy), self.node(cx, cy, sx + 1, sy),
              self.node(cx, cy, sx, sy + 1), self.node(cx, cy, sx + 1, sy + 1)]
        if any(v is None for v in vs):
            return None
        return sum(vs) / 4.0


SENT = -1.0e29

def rect(o, land, x0, y0, x1, y1, name, verbose=False):
    gx0 = int(x0 // 128); gx1 = int((x1 - 1) // 128)
    gy0 = int(y0 // 128); gy1 = int((y1 - 1) // 128)
    rows = []
    for gy in range(gy0, gy1 + 1):
        for gx in range(gx0, gx1 + 1):
            ix, iy = gx - o['gx0'], gy - o['gy0']
            if not (0 <= ix < o['gw'] and 0 <= iy < o['gh']):
                continue
            k = iy * o['gw'] + ix
            hi = o['mx'][k]
            if hi < SENT:
                continue
            lo = o['mn'][k]
            t = land.square(gx, gy)
            rows.append((gx, gy, lo, hi, t))
    print('--- %s  x %d..%d y %d..%d  squares %dx%d, occupied %d'
          % (name, x0, x1, y0, y1, gx1 - gx0 + 1, gy1 - gy0 + 1, len(rows)))
    if not rows:
        print('    EMPTY: no object over any square in this rectangle')
        return
    cl = [r[2] - r[4] for r in rows if r[4] is not None]
    wall = [c for c in cl if c <= 0]
    low = [c for c in cl if 0 < c <= 432]
    high = [c for c in cl if c > 432]
    print('    minZ  %.1f .. %.1f      maxZ  %.1f .. %.1f' %
          (min(r[2] for r in rows), max(r[2] for r in rows),
           min(r[3] for r in rows), max(r[3] for r in rows)))
    print('    terrain %.1f .. %.1f' % (min(r[4] for r in rows if r[4] is not None),
                                        max(r[4] for r in rows if r[4] is not None)))
    print('    clearance (minZ - terrain)  %.1f .. %.1f   mean %.1f' %
          (min(cl), max(cl), sum(cl) / len(cl)))
    print('    WALL squares (clearance <= 0)          %d' % len(wall))
    print('    CEILING below the 432 crossover        %d  (the law DARKENS these)' % len(low))
    print('    CEILING above the 432 crossover        %d  (the law BRIGHTENS these)' % len(high))
    if verbose:
        for r in rows[:verbose]:
            print('      g(%d,%d) w(%d,%d) min %.1f max %.1f terrain %s clr %s'
                  % (r[0], r[1], r[0] * 128, r[1] * 128, r[2], r[3],
                     '%.1f' % r[4] if r[4] is not None else 'none',
                     '%.1f' % (r[2] - r[4]) if r[4] is not None else 'none'))


def main():
    o = read_objh(os.path.join(S, 'probe', 'objh_after.bin'))
    print('objh: g(%d,%d) %dx%d cell %.1f, minZ plane %s'
          % (o['gx0'], o['gy0'], o['gw'], o['gh'], o['cell'],
             'PRESENT' if o['two'] else 'ABSENT'))
    land = Land(os.path.join(S, 'probe', 'land.bin'))
    print('land: cells %dx%d from (%d,%d)' % (land.cw, land.ch, land.mnx, land.mny))
    R = [
        ('a-deck   (elevated highway)', 19712, -41856, 20992, -40576),
        ('a2-brief (the brief coord)', 24644, -41556, 25156, -41044),
        ('a3-catwalk', 31616, -41472, 32640, -40576),
        ('c-open   (control)', 23936, -34816, 24192, -34560),
        ('d-pierfoot', 31232, -45568, 31616, -44160),
    ]
    for nm, x0, y0, x1, y1 in R:
        rect(o, land, x0, y0, x1, y1, nm,
             verbose=8 if 'pierfoot' in nm or 'catwalk' in nm else 0)

    # ---- the whole chunk, as a population: how the law's two sides split
    print()
    x0, y0, x1, y1 = 16384, -49152, 32768, -32768
    n = w = lowc = highc = 0
    nolandc = 0
    for gy in range(y0 // 128, y1 // 128):
        for gx in range(x0 // 128, x1 // 128):
            ix, iy = gx - o['gx0'], gy - o['gy0']
            if not (0 <= ix < o['gw'] and 0 <= iy < o['gh']):
                continue
            k = iy * o['gw'] + ix
            if o['mx'][k] < SENT:
                continue
            n += 1
            t = land.square(gx, gy)
            if t is None:
                nolandc += 1
                continue
            c = o['mn'][k] - t
            if c <= 0:
                w += 1
            elif c <= 432:
                lowc += 1
            else:
                highc += 1
    print('WHOLE CHUNK 4.4.-12  occupied squares %d  (no LAND %d)' % (n, nolandc))
    print('  WALL    clearance <= 0    %d  (%.1f%%)' % (w, 100.0 * w / n))
    print('  CEILING 0 < clr <= 432    %d  (%.1f%%)' % (lowc, 100.0 * lowc / n))
    print('  CEILING clr > 432         %d  (%.1f%%)' % (highc, 100.0 * highc / n))
    print('  census bar (clr > 128) for comparison: objAoSlabSquares counts these')
    b = 0
    for gy in range(y0 // 128, y1 // 128):
        for gx in range(x0 // 128, x1 // 128):
            ix, iy = gx - o['gx0'], gy - o['gy0']
            if not (0 <= ix < o['gw'] and 0 <= iy < o['gh']):
                continue
            k = iy * o['gw'] + ix
            if o['mx'][k] < SENT:
                continue
            t = land.square(gx, gy)
            if t is not None and o['mn'][k] > t + 128.0:
                b += 1
    print('  clearance > 128           %d' % b)


if __name__ == '__main__':
    main()
