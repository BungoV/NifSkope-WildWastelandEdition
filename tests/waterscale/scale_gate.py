"""lane FIX1101A (LATE ITEM 1, bungo 10-31: "make sure the water texture you use has the correct scale, and looks good").
Every normal source the near water draws at spot 5 (the Boston sea, WATR ExtOceanWater) and spot 6 (the Charles at
Cutler Bend, ExtRiverCharlesUpper), in METRES, against real water's bands: capillary / short chop 1-10 cm, wind chop
0.1-1 m, swell / long waves 10 m and more (the sea). Sources and numbers:
  WATR noise layers   the record's NAM2-4 at its uv scales (Fallout4.esm DNAM, notes/water4/tools/watr_all.txt), 512
                      texels a tile; their slope energy's median wavelength in texels MEASURED from the game's textures
                      (watr_textures.json, notes/fix1101a/water/tools/texstats.py: derived statistics, no texture data)
  wave spectrum       src/gl/oceanspectrum.h set tiles (sea 1187 / 97.1 / 15.3 m, fresh 61.9 / 9.53 / 1.47 m) on
                      128 texels: resolved down to 2 texels
  detail octaves      res/shaders/water_detail.glsl (this tree): the record's finest texture at its scale / kWwDetA, B
On a baked body the spectrum REPLACES the record's layers (fo4_water.frag oc.w), so its sources there are the spectrum
and the detail octaves.
  band    each spot draws a source with its median wavelength in each band (the sea also >= 10 m)
          red --red nodetail (WW_CELL_WATER_OCEAN_RED=nodetail, today's code): the sea has nothing under 10 cm -> FAIL
  repeat  the detail's strongest repeat: at every lattice vector of either octave (up to 3 tiles) the two octaves'
          mean slope autocorrelation (the game's 2D texture autocorrelation, watr_textures.json) <= 0.6
          red --red single (one unturned octave, the way the game tiles its layers): 1.0 at its tile -> FAIL
usage: python -I scale_gate.py [--red nodetail|single]"""
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
RED = sys.argv[2] if len(sys.argv) > 2 and sys.argv[1] == '--red' else ''
TEX = json.load(open(os.path.join(HERE, 'watr_textures.json')))
U = 0.0142875   # m a game unit
src = open(os.path.join(TREE, 'res', 'shaders', 'water_detail.glsl')).read()
kA = float(re.search(r'kWwDetA = ([0-9.]+)', src).group(1))
kB = float(re.search(r'kWwDetB = ([0-9.]+)', src).group(1))
rotA = [float(x) for x in re.search(r'kWwDetRotA = vec2\( ([-0-9.]+), ([-0-9.]+) \)', src).groups()]
rotB = [float(x) for x in re.search(r'kWwDetRotB = vec2\( ([-0-9.]+), ([-0-9.]+) \)', src).groups()]
REC = {   # NAM2, NAM3, NAM4 and the uv scales (u a tile)
    'sea (ExtOceanWater)': (('DefaultWater', 'DefaultWaterTile', 'DefaultWaterTile'), (10000.0, 761.0, 217.0), (1187.0, 97.1, 15.3)),
    'river (ExtRiverCharlesUpper)': (('DefaultWater', 'ChurningWaterTile', 'DefaultWaterTile'), (3333.0, 942.0, 181.0),
                                     (61.9, 9.53, 1.47))}
BANDS = (('capillary / short chop', 0.01, 0.10), ('wind chop', 0.10, 1.0), ('swell', 10.0, 1e9))
fails = 0
print('TEXTURE SCALES (median wavelength of the slope energy; tile; texel), metres')
for spot, (names, uv, casc) in REC.items():
    srcs = []
    for i, (nm, s) in enumerate(zip(names, uv)):
        t = TEX[nm]
        med = t['median_wavelength_texels'] * s / t['texels'] * U
        print('  %-28s WATR layer %d %-18s tile %8.3f texel %.4f median %.4f  (replaced by the spectrum on baked water)' % (
            spot, i + 1, nm, s * U, s / t['texels'] * U, med))
    for c, L in enumerate(casc):
        lo, hi = 2 * L / 128.0, L
        srcs.append(('spectrum cascade %d' % c, math.sqrt(lo * hi), lo, hi))
        print('  %-28s spectrum cascade %d           tile %8.3f texel %.4f band %.3f .. %.3f' % (spot, c, L, L / 128, lo, hi))
    if spot.startswith('sea') and RED != 'nodetail':
        t = TEX[names[2]]
        for nm_, k in (('A', kA), ('B', kB)):
            tile = uv[2] / k
            med = t['median_wavelength_texels'] * tile / t['texels'] * U
            srcs.append(('detail octave %s' % nm_, med, med, med))
            print('  %-28s detail octave %s (NAM4 / %.1f) tile %8.3f texel %.4f median %.4f' % (spot, nm_, k, tile * U, tile / t['texels'] * U, med))
    for bn, lo, hi in BANDS:
        if bn == 'swell' and not spot.startswith('sea'):
            continue
        hit = [n for n, m, a, b in srcs if (a <= hi and b >= lo)]
        ok = bool(hit)
        fails += not ok
        print('BAND %-28s %-24s %.2f .. %g m: %s  %s' % (spot, bn, lo, hi, ', '.join(hit) if hit else 'NOTHING', 'PASS' if ok else 'FAIL'))

# repeat: the 2D autocorrelation of the detail (two octaves, turned), at the lattice vectors of each
ac = TEX['DefaultWaterTile']['ac2d_step4']
n4 = len(ac)


def rho(lag_tex):
    """the texture's slope autocorrelation at a lag (texels, 2D), circular, bilinear on the 4-texel table"""
    x = (lag_tex[0] / 4.0) % n4
    y = (lag_tex[1] / 4.0) % n4
    x0, y0 = int(math.floor(x)), int(math.floor(y))
    fx, fy = x - x0, y - y0
    g = lambda i, j: ac[j % n4][i % n4]  # noqa: E731
    return (g(x0, y0) * (1 - fx) * (1 - fy) + g(x0 + 1, y0) * fx * (1 - fy) + g(x0, y0 + 1) * (1 - fx) * fy
            + g(x0 + 1, y0 + 1) * fx * fy)


def tex_lag(world, rot, k, tile):
    """a world lag (u) in an octave's texels: turned by the octave's angle, scaled by k / tile x 512"""
    c, s = rot
    r = (c * world[0] - s * world[1], s * world[0] + c * world[1])
    return (r[0] * k / tile * 512.0, r[1] * k / tile * 512.0)


tile0 = REC['sea (ExtOceanWater)'][1][2]
if RED == 'single':
    octs = [((1.0, 0.0), kA)]
else:
    octs = [(tuple(rotA), kA), (tuple(rotB), kB)]
worst, at = 0.0, None
for (rot, k) in octs:
    T = tile0 / k   # u
    c, s = rot
    for i in range(-3, 4):
        for j in range(-3, 4):
            if i == 0 and j == 0:
                continue
            # a lattice vector of this octave, in world units (its texture lattice turned back)
            vx, vy = i * T, j * T
            world = (c * vx + s * vy, -s * vx + c * vy)
            r = sum(rho(tex_lag(world, rot2, k2, tile0)) for rot2, k2 in octs) / len(octs)
            if r > worst:
                worst, at = r, (world[0] * U, world[1] * U)
ok = worst <= 0.6
fails += not ok
print('REPEAT the detail\'s strongest repeat %.3f at a lag of (%.3f, %.3f) m (%d octave%s)  %s' % (
    worst, at[0], at[1], len(octs), '' if len(octs) == 1 else 's', 'PASS' if ok else 'FAIL'))
print('VERDICT %s' % ('PASS' if fails == 0 else 'FAIL (%d)' % fails))
