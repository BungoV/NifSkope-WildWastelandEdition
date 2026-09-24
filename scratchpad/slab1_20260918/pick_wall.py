# Lane SLAB1 -- PICK THE WALL REFUTER, FROM GEOMETRY ALONE.
#
# The pre-registered refuter wants ground beside an occluder that REACHES THE
# GROUND: the ceiling branch must not have become "ignore objects", so such a
# rectangle must not brighten.  My first attempt (d-pierfoot) was chosen from a
# placement name and turned out to sit UNDER the bridge deck, so the squares its
# march reads are deck plates, not piles -- it was never a wall test.
#
# This script picks the rectangle from `probe/objh_after.bin` + `probe/land.bin`
# BEFORE any mask is opened, on three conditions:
#   1. every square of the rectangle itself is EMPTY (the texels are ground),
#   2. every occupied square the march can reach from it (the 1458-unit ring)
#      is a WALL -- its MIN object surface is at or below the rectangle's own
#      terrain, so both laws must take the same branch for all of them,
#   3. there are enough of those walls, and they are high enough, that the
#      rectangle is measurably dark to begin with (otherwise "did not move" is
#      trivially true of an unshaded sample).
# Under those conditions the new law is ALGEBRAICALLY the old one, so the
# measurement is a known-answer test on the shipped bytes.
import math, os, sys
from clearance import read_objh, Land, SENT

# Two readings of "the occluder reaches the ground", and the lane measures both:
#   default   STRICT: min <= the RECTANGLE's own terrain, which is the march's
#             own `lo <= h0` test, so every square takes the wall branch under
#             both laws and the rectangle is branch-identical BY CONSTRUCTION.
#   --own     THE BRIEF's words ("a model whose triangles reach the ground"):
#             min <= the OCCLUDER SQUARE's own terrain. On sloping ground an
#             uphill building satisfies this and still reads as a ceiling from
#             below, so this rectangle MAY move -- and by how much is the
#             number that says whether that consequence is small.
OWN = '--own' in sys.argv

S = os.path.dirname(os.path.abspath(__file__))
CELL = 128.0
DIRS = [(1,0),(-1,0),(0,1),(0,-1),(0.7071,0.7071),(0.7071,-0.7071),(-0.7071,0.7071),(-0.7071,-0.7071)]

# The chunk the bakes wrote, in lattice squares.
GX0, GY0, GX1, GY1 = 16384 // 128, -49152 // 128, 32768 // 128, -32768 // 128


def main():
    o = read_objh(os.path.join(S, 'probe', 'objh_after.bin'))
    land = Land(os.path.join(S, 'probe', 'land.bin'))
    W = 4          # rectangle side, in squares (4 * 128 = 512 world units)
    REACH = 12     # 1458 / 128 = 11.4 squares, rounded up
    best = None
    for gy in range(GY0, GY1 - W + 1):
        for gx in range(GX0, GX1 - W + 1):
            # -- 1. the rectangle itself must be empty, and have LAND
            ok = True
            hs = []
            for y in range(gy, gy + W):
                for x in range(gx, gx + W):
                    ix, iy = x - o['gx0'], y - o['gy0']
                    if not (0 <= ix < o['gw'] and 0 <= iy < o['gh']):
                        ok = False; break
                    if o['mx'][iy * o['gw'] + ix] > SENT:
                        ok = False; break
                    t = land.square(x, y)
                    if t is None:
                        ok = False; break
                    hs.append(t)
                if not ok:
                    break
            if not ok:
                continue
            h0 = sum(hs) / len(hs)
            # -- 2/3. THE SQUARES THE MARCH ACTUALLY READS, not a bounding
            # ring: the union, over a 32-unit sample grid inside the rectangle,
            # of the 8 directions x 7 steps the march visits. Anything outside
            # that set cannot change this rectangle whatever law is used.
            seen = set()
            yy = gy * CELL + 16.0
            while yy < ( gy + W ) * CELL:
                xx = gx * CELL + 16.0
                while xx < ( gx + W ) * CELL:
                    for dx, dy in DIRS:
                        d = CELL
                        while d <= 2048.0:
                            seen.add( ( int( math.floor( ( xx + dx * d ) / CELL ) ),
                                        int( math.floor( ( yy + dy * d ) / CELL ) ) ) )
                            d *= 1.5
                    xx += 32.0
                yy += 32.0
            walls = 0
            worst = 0.0       # the steepest wall, as a tangent: how dark this is
            for ( x, y ) in seen:
                ix, iy = x - o['gx0'], y - o['gy0']
                if not ( 0 <= ix < o['gw'] and 0 <= iy < o['gh'] ):
                    ok = False; break
                k = iy * o['gw'] + ix
                hi = o['mx'][k]
                if hi < SENT:
                    continue
                lo = o['mn'][k]
                if OWN:
                    ts = land.square( x, y )
                    if ts is None or lo > ts:
                        ok = False; break
                elif lo > h0:          # a CEILING relative to this ground
                    ok = False; break
                walls += 1
                dist = max( CELL, CELL * max( abs( x - gx ), abs( y - gy ) ) )
                worst = max( worst, ( hi - h0 ) / dist )
            if not ok or walls < 8:
                continue
            score = (worst, walls)
            if best is None or score > best[0]:
                best = (score, gx, gy, walls, worst, h0)
    if best is None:
        print('no rectangle satisfies all three conditions')
        return
    (_, gx, gy, walls, worst, h0) = best
    print('WALL REFUTER (%s) (picked from geometry, no mask opened):'
          % ('own-ground test, the brief wording' if OWN else 'strict, the march own lo<=h0 test'))
    print('  lattice squares g(%d..%d, %d..%d)' % (gx, gx + W - 1, gy, gy + W - 1))
    print('  world rectangle x %d..%d  y %d..%d  (%d x %d units)'
          % (gx * 128, (gx + W) * 128, gy * 128, (gy + W) * 128, W * 128, W * 128))
    print('  terrain under it %.1f' % h0)
    print('  occupied squares within the 1458-unit reach: %d, ALL walls (min <= %.1f)' % (walls, h0))
    print('  steepest wall tangent %.3f  ->  it is genuinely shaded, not open sky' % worst)


if __name__ == '__main__':
    main()
