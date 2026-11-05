"""lane FIX1031B (WATERLOOK1 overhaul, LATE ITEMs 1/3/5.2/6.1): the clear colour per depth step vs Boston's target band.

bungo 10-31: the Boston sea read tropical blue + turquoise shallows (bungo_sea_color_choppy.webp), the river's shallows
by the sunken boat glowed lime (sheet_W0river). Target (RESEARCH.md section 3): the colour comes from the cell's own WATR
deep colour, shaped by coastal Beer-Lambert absorption with depth: the bed visible only in the first 1-2 m, then the
record's grey-green; no saturated lime or turquoise.
This runs the REAL water_clear.glsl (wwWaterClear, wwClearK) through the driver: a lit bed (albedo 0.25 sand, the scene
pixel the water covers: albedo x (sun cos + sky)) under z m of water, seen 45 deg down (along = z / cos 45), the sun at
33.5 deg (spot 6, CommonwealthClear 12:00), sky (0.35, 0.45, 0.60), sun (2.5, 2.4, 2.2) linear (REASONED values).
Display: colour x 0.45 / (dry sand in air) then gamma 2.2 (dry sand = 178 levels); hue / saturation are scale-free.
Gates (each per class: sea = ExtOceanWater, river = ExtRiverCharlesUpper, lake = ExtLakeWater):
  sat     every depth step HSV saturation <= 0.50 and never lime (hue 70-140) or turquoise (160-200) above 0.35
          (the Boston records; a modder's own saturated record is judged by its hue only)
  hue     deep water (no bed) within 30 deg of the record's own deep colour hue; also for a modder's blue record
          (MassFusionWaterBlue deep 19, 41, 55): the record drives it
  bed     the bed's share of the light: sea >= 0.20 at 1 m and <= 0.10 at 3 m; river >= 0.20 at 0.5 m, <= 0.10 at 2 m;
          lake >= 0.20 at 1 m, <= 0.10 at 3 m (Secchi 2.5 / 1.2 / 2.0 m: the bed fades within 1-2 m)
VERDICT PASS = all. --red 4 = WW_CELL_WATER_CLEAR_RED=old (today's clear colour) must FAIL.
python -I colour_gate.py [--shaders DIR] [--red N] [--table]
"""
import argparse
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import glh  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(glh.TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
a = ap.parse_args()

DEPTHS = (0.25, 0.5, 1.0, 2.0, 3.0, 5.0, 10.0, -1.0)   # m; -1 = no bed (deep)
CASES = (('sea', 0, (46, 61, 57)), ('river', 1, (47, 51, 43)), ('lake', 2, (43, 51, 50)), ('sea/modder', 0, (19, 41, 55)))
SUN, SKY, ELEV, ALB = (2.5, 2.4, 2.2), (0.35, 0.45, 0.60), 33.5, 0.25
UM = 0.0142875
FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_depth.glsl')
        + '\n' + glh.glsl(a.shaders, 'water_clear.glsl') + r"""
uniform vec4 depths[2];
uniform vec4 deepCol[4];
uniform ivec4 cls;
uniform vec3 sunC, skyC;
uniform float sunCos, alb, mu;
uniform int withBed;
out vec4 oc;
void main()
{
	int id = int( gl_FragCoord.x ), ic = int( gl_FragCoord.y );
	float zm = depths[id / 4][id % 4];
	wwClearIn.cls = cls[ic];
	wwClearIn.sun = sunC;
	wwClearIn.sunCos = sunCos;
	wwClearIn.sky = skyC;
	wwClearIn.viewMu = mu;
	wwClearDeep = deepCol[ic].rgb;
	vec3 bedLit = withBed != 0 ? alb * ( sunC * sunCos + skyC ) : vec3( 0.0 );
	float vert = zm < 0.0 ? 1e9 : zm / 0.0142875, along = zm < 0.0 ? 1e9 : vert / mu;
	oc = vec4( wwWaterClear( bedLit, along, vert ), 1.0 );
}
""")
P = glh.Prog(FRAG, 8, 4)
P.set4v('depths', [DEPTHS[0:4], DEPTHS[4:8]])
P.set4v('deepCol', [tuple((c / 255.0) ** 2.2 for c in d) + (0.0,) for _, _, d in CASES])
P.set('sunC', *SUN)
P.set('skyC', *SKY)
P.set('sunCos', math.sin(math.radians(ELEV)))
P.set('alb', ALB)
P.set('mu', math.cos(math.radians(45.0)))
P.set('waterClearRed', a.red)
ivec = glh._fn('glUniform4i', None, glh.GLint, glh.GLint, glh.GLint, glh.GLint, glh.GLint)
ivec(P.loc('cls'), *[c for _, c, _ in CASES])
P.set('withBed', 1)
full = P.draw()[..., :3].astype(np.float64)
P.set('withBed', 0)
scat = P.draw()[..., :3].astype(np.float64)

sand = ALB * (np.array(SUN) * math.sin(math.radians(ELEV)) + np.array(SKY))
expo = 0.45 / sand.max()
lum = np.array([0.2126, 0.7152, 0.0722])
disp = glh.srgb(full * expo)
H, Sat = glh.hsv(disp)
print('arm: shaders %s, waterClearRed %d' % (a.shaders, a.red))
print('display: dry sand in air = (%d, %d, %d)' % tuple(glh.srgb(sand * expo)))
ok = True
BED = {'sea': (1.0, 3.0), 'river': (0.5, 2.0), 'lake': (1.0, 3.0), 'sea/modder': (1.0, 3.0)}
for ic, (name, c, deep) in enumerate(CASES):
    dh, ds = glh.hsv(np.array(deep, float))
    print('%s (class %d, WATR deep %s: hue %.0f sat %.2f)' % (name, c, deep, dh, ds))
    for idp, z in enumerate(DEPTHS):
        rgb = disp[ic, idp]
        share = 0.0 if z < 0 else 1.0 - float(scat[ic, idp] @ lum) / max(float(full[ic, idp] @ lum), 1e-12)
        h, s = float(H[ic, idp]), float(Sat[ic, idp])
        bad = s > 0.50 or (s > 0.35 and (70 <= h <= 140 or 160 <= h <= 200))
        if name == 'sea/modder':
            bad = False
        note = ''
        if bad:
            note = ' SATURATED' if s > 0.50 else (' LIME' if h < 140 else ' TURQUOISE')
        if z < 0:
            dhue = abs((h - float(dh) + 180) % 360 - 180)
            if dhue > 30:
                bad = True
                note += ' HUE off the record by %.0f' % dhue
        lo, hi = BED[name]
        if z == lo and share < 0.20:
            bad, note = True, note + ' bed hidden too soon'
        if z == hi and share > 0.10:
            bad, note = True, note + ' bed still shows'
        ok &= not bad
        print('  %-6s lin (%.4f %.4f %.4f) display (%3d %3d %3d) hue %5.1f sat %.2f bed share %.2f%s' % (
            'deep' if z < 0 else '%.2fm' % z, *full[ic, idp], *rgb.round().astype(int), h, s, share, note))
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
