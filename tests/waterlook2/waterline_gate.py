"""lane FIX1031B (WATERLOOK1 overhaul, LATE ITEM 6.2): the choppy shore (triangular teeth in the clear shallows).

bungo 10-31 (bungo_sea_color_choppy.webp, sheet_W0sea waterline 4x): with the clear colour on, the shallows showed
straight-edged teeth: a smooth beige ramp (the baked bed's albedo) cut by flat grey triangles (the drawn ground) with a
dark triangle at their base. The first agent localised it (HANDOFF item 4): water_depth.glsl wwWaterBedAt switches
from the drawn scene to the baked bed where the scene lies more than 256 u under the bake, and the drawn ground under
the near sea is a coarse mesh (far LOD suspected) whose triangles cross that line.
MODEL (REASONED geometry, the real colour functions): a beach sloping 1:25 under the water (the bake's bed depth
bd = 0.04 x, x = distance from the waterline, u), the drawn ground = a mesh of 1024 u triangles whose vertices carry
the true bed depth + a coarse-LOD height error (normal, mean 250 u deeper, sd 200 u, seed 7: the coarse vertices
cannot follow the shallow beach and straddle the 256 u step, as the crop's grey teeth + dark bases read); seen at a grazing 11.5 deg (V.z -0.2, spot 5's
eye 760 u over the sea at ~3500 u); the drawn ground's colour = LOD albedo 0.18 grey lit; the baked bed = the class's
bed albedo lit. Each water sample runs fo4_water.frag main's bed choice (transcribed: the soft blend wwWaterBedBakeW on,
the hard step `bed.baked ?` for red old) with the REAL water_depth.glsl / water_clear.glsl through the driver.
Waterline roughness = hard edges along the shore: neighbours 4 u apart ALONG the shore (no depth change between them)
whose display colour differs by > 8 levels, counted per km of shore over the first 1536 u of water.
VERDICT PASS = edges <= 2 per km and the p99.9 neighbour step <= 6 levels. --red 4 (WW_CELL_WATER_CLEAR_RED=old, today's
clear colour) must FAIL (bar registered before the first run: red >= 20 per km).
python -I waterline_gate.py [--shaders DIR] [--red N]
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

NX, NY, DX, DY = 512, 1024, 3.0, 4.0     # across the shore (x) 1536 u, along it (y) 4096 u
SUN, SKY, ELEV, VZ = (2.5, 2.4, 2.2), (0.35, 0.45, 0.60), 33.5, -0.2
rng = np.random.default_rng(7)
G = 1024.0
gx = np.arange(0.0, NX * DX + G, G)
gy = np.arange(0.0, NY * DY + G, G)
err = rng.normal(250.0, 200.0, (len(gy), len(gx)))   # coarse vertices sit deeper than the shallow beach, around the step
vz = 0.04 * gx[None, :] + err                       # the coarse mesh's vertex depths (u under the surface)
xs = (np.arange(NX) + 0.5) * DX
ys = (np.arange(NY) + 0.5) * DY
X, Y = np.meshgrid(xs, ys)
i, j = np.floor(X / G).astype(int), np.floor(Y / G).astype(int)
fx, fy = X / G - i, Y / G - j
v00, v10, v01, v11 = vz[j, i], vz[j, i + 1], vz[j + 1, i], vz[j + 1, i + 1]
lower = fx + fy <= 1.0                              # two triangles a quad (diagonal 10-01)
scene = np.where(lower, v00 + fx * (v10 - v00) + fy * (v01 - v00), v11 + (1 - fx) * (v01 - v11) + (1 - fy) * (v10 - v11))
bd = 0.04 * X
land = scene <= 0.0                                  # the coarse ground above the surface: no water drawn there
data = np.stack([bd, np.maximum(scene, 0.0), np.zeros_like(bd), np.zeros_like(bd)], -1).astype(np.float32)

FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_depth.glsl')
        + '\n' + glh.glsl(a.shaders, 'water_clear.glsl') + r"""
uniform sampler2D geo;
uniform vec3 sunC, skyC;
uniform float sunCos, vz;
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
	vec3 refr = 0.18 * ( sunC * sunCos + skyC );	// the drawn ground behind the water
	// wwWaterBedAt's baked test (the hard step) and fo4_water.frag main's choice
	float bd = g.x;
	bool baked = bd >= 0.0 && vert > bd + 256.0;
	if ( waterClearRed != 4 ) {
		float bedW = wwWaterBedBakeW( bd, vert, true );
		vec3 bedC = bedW > 0.0 ? depthFogC( wwWaterClearBed(), bd / -vz, bd ) : vec3( 0.0 );
		refr = bedW >= 1.0 ? bedC : mix( depthFogC( refr, along, vert ), bedC, bedW );
	} else
		refr = baked ? depthFogC( wwWaterClearBed(), bd / -vz, bd ) : depthFogC( refr, along, vert );
	oc = vec4( refr, baked ? 1.0 : 0.0 );
}
""")
P = glh.Prog(FRAG, NX, NY)
P.tex('geo', data)
P.set('sunC', *SUN)
P.set('skyC', *SKY)
P.set('sunCos', math.sin(math.radians(ELEV)))
P.set('waterClearRed', a.red)
sand = 0.25 * (np.array(SUN) * math.sin(math.radians(ELEV)) + np.array(SKY))
km = NY * DY * 0.0142875 / 1000.0                    # shore length in km
ok_pair = ~(land[1:] | land[:-1])
print('arm: shaders %s, waterClearRed %d' % (a.shaders, a.red))
good = True
# spot 5's grazing view, and a steeper one (30 deg: the bed shows more, so the blend itself is tested)
for vzv in (VZ, -0.5):
    P.set('vz', vzv)
    out = P.draw().astype(np.float64)
    disp = glh.srgb(out[..., :3] * 0.45 / sand.max())
    step = np.abs(np.diff(disp, axis=0)).max(-1)    # along the shore (y neighbours, 4 u)
    st = step[ok_pair]
    edges = int((st > 8.0).sum())
    per_km = edges / km / NX                         # per km of one line along the shore
    p999 = float(np.percentile(st, 99.9))
    print('view V.z %.2f (%.1f deg down)' % (vzv, math.degrees(math.asin(-vzv))))
    print('  model: %.3f km of shore, %d water samples (%.1f %% coarse ground above the surface), baked bed wins (hard '
          'rule) on %.1f %%' % (km, int((~land).sum()), 100 * land.mean(), 100 * out[..., 3][~land].mean()))
    print('  along-shore neighbour steps: mean %.2f p99 %.2f p99.9 %.2f max %.1f levels; hard edges (> 8) %d = %.1f per km'
          % (st.mean(), np.percentile(st, 99), p999, st.max(), edges, per_km))
    for xb in (0, 128, 256, 511):
        col = disp[:, xb][~land[:, xb]]
        if len(col):
            print('    %5.0f u from the waterline: display mean (%3.0f %3.0f %3.0f) sd along the shore %.1f' % (
                xs[xb], *col.mean(0), col.std(0).max()))
    good &= per_km <= 2.0 and p999 <= 6.0
print('VERDICT %s' % ('PASS' if good else 'FAIL'))
sys.exit(0 if good else 1)
