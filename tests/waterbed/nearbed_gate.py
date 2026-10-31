"""lane FIX1101A (LATE ITEM 7, bungo 10-31: "isn't the lod water too imprecise?"): near water takes its bed per pixel
from the drawn scene; the 128 u bake only past the loaded cells, blended, or where no bed is drawn.
MODEL (REASONED geometry, the REAL water_depth.glsl / water_clear.glsl through the driver; tests/waterlook2's
waterline model): a beach sloping 1:25 under the water (the bake's bed bd = 0.04 x, u), the drawn ground a mesh of
1024 u triangles whose vertices carry the bed + a coarse height error (mean 250 u deeper, sd 200 u, seed 7); the drawn
ground's colour = LOD albedo 0.18 grey lit, the baked bed = the class's bed albedo lit; seen 11.5 deg down. The bed choice
is fo4_water.frag main's (the clear colour path), weight wwWaterBedNearW( bd, scene, dist ).
  depth   at the near distance (3500 u, spot 5's sea) the bed the colour uses is the scene's (the depth the frame drew):
          mean |used - scene| <= 2 u. red --red 6 (WW_CELL_WATER_CLEAR_RED=bake, today's rule): the bake wins where the
          drawn ground lies 64..768 u under it -> far from the scene -> FAIL
  edges   at the near distance, the along-shore hard edges (display steps > 8 levels between neighbours 4 u apart):
          <= 2 per km (the waterline's teeth: the colour switches where the baked and the drawn bed meet)
  seam    the same bed sample seen from 6000 to 22000 u: the display colour changes by <= 3 levels per 256 u of view
          distance (the near-to-far blend has no step)
python -I nearbed_gate.py [--red N]"""
import argparse
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'waterlook2'))
import glh  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(glh.TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
a = ap.parse_args()

NX, NY, DX, DY = 512, 1024, 3.0, 4.0
SUN, SKY, ELEV, VZ = (2.5, 2.4, 2.2), (0.35, 0.45, 0.60), 33.5, -0.2
rng = np.random.default_rng(7)
G = 1024.0
gx = np.arange(0.0, NX * DX + G, G)
gy = np.arange(0.0, NY * DY + G, G)
err = rng.normal(250.0, 200.0, (len(gy), len(gx)))
vz = 0.04 * gx[None, :] + err
xs = (np.arange(NX) + 0.5) * DX
ys = (np.arange(NY) + 0.5) * DY
X, Y = np.meshgrid(xs, ys)
i, j = np.floor(X / G).astype(int), np.floor(Y / G).astype(int)
fx, fy = X / G - i, Y / G - j
v00, v10, v01, v11 = vz[j, i], vz[j, i + 1], vz[j + 1, i], vz[j + 1, i + 1]
lower = fx + fy <= 1.0
scene = np.where(lower, v00 + fx * (v10 - v00) + fy * (v01 - v00), v11 + (1 - fx) * (v01 - v11) + (1 - fy) * (v10 - v11))
bd = 0.04 * X
land = scene <= 0.0
data = np.stack([bd, np.maximum(scene, 0.0), np.zeros_like(bd), np.zeros_like(bd)], -1).astype(np.float32)

FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_depth.glsl')
        + '\n' + glh.glsl(a.shaders, 'water_clear.glsl') + r"""
uniform sampler2D geo;
uniform vec3 sunC, skyC;
uniform float sunCos, vz, distU;
out vec4 oc;
vec3 depthFogC( vec3 c, float along, float vert ) { return wwWaterClear( c, along, vert ); }
void main()
{
	vec4 g = texelFetch( geo, ivec2( gl_FragCoord.xy ), 0 );
	wwClearIn.cls = 0;
	wwClearIn.sun = sunC;
	wwClearIn.sunCos = sunCos;
	wwClearIn.sky = skyC;
	wwClearIn.viewMu = -vz;
	wwClearDeep = pow( vec3( 46.0, 61.0, 57.0 ) / 255.0, vec3( 2.2 ) );
	float vert = g.y, along = vert / -vz;
	vec3 refr = 0.18 * ( sunC * sunCos + skyC );
	float bd = g.x;
	// fo4_water.frag main's clear-colour bed choice
	float bedW = wwWaterBedNearW( bd, vert, distU, waterClearRed != 6 );
	vec3 bedC = bedW > 0.0 ? depthFogC( wwWaterClearBed(), bd / -vz, bd ) : vec3( 0.0 );
	refr = bedW >= 1.0 ? bedC : mix( depthFogC( refr, along, vert ), bedC, bedW );
	// the depth the colour stands for: the weights' mix of the two beds
	oc = vec4( refr, mix( vert, bd, bedW ) );
}
""")
P = glh.Prog(FRAG, NX, NY)
P.tex('geo', data)
P.set('sunC', *SUN)
P.set('skyC', *SKY)
P.set('sunCos', math.sin(math.radians(ELEV)))
P.set('waterClearRed', a.red)
P.set('vz', VZ)
sand = 0.25 * (np.array(SUN) * math.sin(math.radians(ELEV)) + np.array(SKY))
km = NY * DY * 0.0142875 / 1000.0
ok_pair = ~(land[1:] | land[:-1])
water = ~land
print('arm: shaders %s, waterClearRed %d' % (a.shaders, a.red))
fails = 0

P.set('distU', 3500.0)
out = P.draw().astype(np.float64)
derr = float(np.abs(out[..., 3] - scene)[water].mean())
ok = derr <= 2.0
fails += not ok
print('DEPTH near (3500 u): mean |used bed - scene| %.1f u over %d water samples (scene vs bake mean |d| %.0f u)  %s' % (
    derr, int(water.sum()), float(np.abs(scene - bd)[water].mean()), 'PASS' if ok else 'FAIL'))
disp = glh.srgb(out[..., :3] * 0.45 / sand.max())
st = np.abs(np.diff(disp, axis=0)).max(-1)[ok_pair]
per_km = int((st > 8.0).sum()) / km / NX
ok = per_km <= 2.0
fails += not ok
print('EDGES near: along-shore hard edges %.1f per km (p99.9 step %.2f levels)  %s' % (per_km, float(np.percentile(st, 99.9)),
      'PASS' if ok else 'FAIL'))

prof = []
for d in np.arange(6000.0, 22001.0, 256.0):
    P.set('distU', float(d))
    o = P.draw().astype(np.float64)
    prof.append(glh.srgb(o[..., :3] * 0.45 / sand.max())[water].mean(0))
prof = np.array(prof)
jump = float(np.abs(np.diff(prof, axis=0)).max())
ok = jump <= 3.0
fails += not ok
print('SEAM near -> far (6000 .. 22000 u): the largest change of the mean display colour per 256 u %.2f levels (near %s, far %s)  %s' % (
    jump, np.round(prof[0]).astype(int), np.round(prof[-1]).astype(int), 'PASS' if ok else 'FAIL'))
print('VERDICT %s' % ('PASS' if fails == 0 else 'FAIL (%d)' % fails))
