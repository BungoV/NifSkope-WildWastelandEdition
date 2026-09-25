# EXTENT1 census (no build): LAND cells per load order vs vanilla non-flat .BTR tiles vs the installed .lodl.
# usage: python census.py
import os, sys, struct, glob
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodl_open_authority import Lodt

def load_dump(p):
    b = open(p, 'rb').read()
    mnx, mny, cw, ch = struct.unpack_from('<4i', b, 0)
    pres = np.frombuffer(b, np.uint8, cw * ch, 16).reshape(ch, cw)
    g = np.frombuffer(b, '<i2', cw * ch * 33 * 33, 16 + cw * ch).reshape(ch, cw, 33, 33)
    return mnx, mny, cw, ch, pres, g

BTR = r'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth'
LODL = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.lodl'
lodl = sys.argv[1] if len(sys.argv) > 1 else LODL

for name in ('land_vanilla.bin', 'land_mo2.bin'):
    mnx, mny, cw, ch, pres, g = load_dump(os.path.join(HERE, name))
    rng = g.reshape(ch, cw, -1).max(-1).astype(int) - g.reshape(ch, cw, -1).min(-1).astype(int)
    ys, xs = np.nonzero(pres)
    print('%s: cells %dx%d from (%d,%d); LAND in %d of %d; LAND bounds x %d..%d y %d..%d; '
          'cells with relief (max-min > 0): %d' % (name, cw, ch, mnx, mny, int(pres.sum()), cw * ch,
          xs.min() + mnx, xs.max() + mnx, ys.min() + mny, ys.max() + mny, int((rng > 0).sum())))
    ys, xs = np.nonzero(rng > 0)
    print('   relief bounds x %d..%d y %d..%d' % (xs.min() + mnx, xs.max() + mnx, ys.min() + mny, ys.max() + mny))
    if name == 'land_mo2.bin':
        M = (mnx, mny, pres, g, rng)

# vanilla level-4 BTR tiles: flat or not, by size; and by LAND relief over the same 4x4 cells
mnx, mny, pres, g, rng = M
files = glob.glob(os.path.join(BTR, 'Commonwealth.4.*.BTR'))
tiles = {}
for f in files:
    p = os.path.basename(f).split('.')
    tiles[(int(p[2]), int(p[3]))] = os.path.getsize(f)
print('vanilla level-4 BTR tiles: %d; sizes min %d max %d' % (len(tiles), min(tiles.values()), max(tiles.values())))
FLAT = 2629
nonflat = {k for k, v in tiles.items() if v > FLAT}
relief = set()
for (tx, ty) in tiles:
    x0, y0 = tx - mnx, ty - mny
    if rng[y0:y0 + 4, x0:x0 + 4].max() > 0:
        relief.add((tx, ty))
print('tiles bigger than the flat-ocean size %d: %d; tiles whose 4x4 cells carry LAND relief: %d' % (FLAT, len(nonflat), len(relief)))
print('   non-flat BTR but no LAND relief: %d; LAND relief but flat-size BTR: %d' % (len(nonflat - relief), len(relief - nonflat)))
xs = [k[0] for k in nonflat]; ys = [k[1] for k in nonflat]
print('   non-flat BTR tile bounds x %d..%d y %d..%d (tile origin cells)' % (min(xs), max(xs), min(ys), max(ys)))
for k in ((-64, 0), (0, 60), (0, -80), (40, 0), (0, 0)):
    x0, y0 = k[0] - mnx, k[1] - mny
    print('   tile %s: BTR %d bytes, LAND relief over its 16 cells max %d units' % (k, tiles.get(k, -1), int(rng[y0:y0+4, x0:x0+4].max()) * 8))

# the installed .lodl: header bounds + interior samples at named outer cells vs LAND
L = Lodt(lodl)
print('lodl %s: header cells [%d,%d]..[%d,%d] spc %d' % (os.path.basename(lodl), L.minX, L.minY, L.maxX, L.maxY, L.spc))
for (cx, cy) in ((-80, 2), (2, 70), (2, -85), (-60, -60), (60, 60)):
    mis = 0; n = 0; worst = 0
    for r in range(1, 32, 3):
        for c in range(1, 32, 3):
            want = int(g[cy - mny, cx - mnx, r, c]) * 8
            gx = (cx - L.minX) * L.spc + c; gy = (cy - L.minY) * L.spc + r
            got = L.height(gx, gy)
            n += 1
            if abs(got - want) > 0.5:
                mis += 1; worst = max(worst, abs(got - want))
    print('   cell (%d,%d): LAND relief %d u, %d samples vs .lodl, %d mismatched, worst %g' % (cx, cy, int(rng[cy - mny, cx - mnx]) * 8, n, mis, worst))
