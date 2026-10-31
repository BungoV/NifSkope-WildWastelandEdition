"""lane FIX1101A (P0, the manager's rvc6_on 10-31 on main 9b443dcf, spot 6 river: "flat grey hard-edged blotches all over the
river (no lace, like paint)" + "a large grey shape on the dry bank below the waterline"). The REAL water_riverfoam.glsl
(wwWaterRiverFoam, wwFoamWet) with water_info.glsl + water_shore.glsl (wwFoamDetail, wwShoreEdgeSoft) through the driver,
one uniform river tile as tests/waterlook2/riverfoam_gate.py (body class 1, flow 0.583 m/s, a 0.30 m deep bed: the
shallows the foam gathers on), the uniforms cellwater.cpp sets, 1024 x 1024 samples 0.25 u apart (a near view: 256 u
square), foot 0.25 u.
  lace   inside the streaks' cores (where the solid law's alpha >= 0.8 of full) at least 25 % of the pixels are holes
         (alpha < 0.3 of full), and no solid foam patch (alpha > half of full, 4-connected) covers more than 0.5 m^2
  land   a bank sloping out of the water (the column over the drawn ground 64 u .. 0 across the strip, the soft edge from
         that water path as fo4_water.frag main has it, V.z -0.2): foam alpha x wwFoamWet must be 0 (< 0.01) on every
         sample whose column is under 4 u (6 cm: the film over the dry bank)
VERDICT PASS = both. Red --red 32 (WW_CELL_WATER_RIVERFOAM_RED=paint: today's solid streaks and foam over the bank) FAILS.
python -I foam2_gate.py [--shaders DIR] [--red N]"""
import argparse
import os
import sys

import numpy as np
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'waterlook2'))
import glh  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(glh.TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
a = ap.parse_args()

N, STEP = 1024, 0.25
WCNT = 64.0
FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_shore.glsl')
        + '\n' + glh.glsl(a.shaders, 'water_riverfoam.glsl') + r"""
uniform float tclock, stepU;
uniform int mode;	// 0 the foam alone, 1 the bank strip (foam x wet)
out vec4 oc;
void main()
{
	vec2 P = vec2( 1000.0, 600.0 ) + gl_FragCoord.xy * stepU;
	vec4 f = wwWaterRiverFoam( P, tclock, stepU );
	if ( mode == 0 ) {
		oc = f;
		return;
	}
	// the bank: the water column over the drawn ground falls from 64 u to 0 across x, then the dry bank (column 0)
	float column = max( 64.0 - 80.0 * gl_FragCoord.x / 1024.0, 0.0 );
	float along = column / 0.2;
	float edge = wwShoreEdgeSoft( vec2( 1e9, 0.0 ), along, -0.2 );
	oc = vec4( f.y * wwFoamWet( edge, column ), column, edge, f.y );
}
""")
P = glh.Prog(FRAG, N, N)
word = 1 | (255 << 16) | (13 << 24)   # body 1, shore steps 255, depth code 13 = 21 u (0.30 m)
P.tex('waterInfoIndex', np.array([[[0, word]]], np.uint32), 'u')
P.tex('waterInfoAtlas', np.zeros((1, 1), np.uint32), 'u')
bodies = np.zeros((1, 256, 4), np.float32)
bodies[0, 1] = (1300.0, 1.0, 0.0, 1000.0)
P.tex('waterInfoBodies', bodies)
fw = (int(round(0.5 * WCNT)) & 0xFFFF) | ((int(round(0.3 * WCNT)) & 0xFFFF) << 16)
P.tex('waterFlowIndex', np.array([[[0, fw]]], np.uint32), 'u')
P.tex('waterFlowAtlas', np.zeros((1, 1), np.uint32), 'u')
P.set('waterInfoGrid', 0.0, 0.0, 128.0, 32.0)
P.set('waterInfoLayout', 1.0, 1.0, 16.0, 1.0)
P.set('waterInfoOn', 1)
P.set('waterFlowGrid', 0.0, 0.0, 128.0, WCNT)
P.set('waterFlowLayout', 1.0, 1.0, 16.0, 1.0)
P.set('waterFlowOn', 1)
P.set('waterRiverFoamOn', 1)
P.set('waterRiverFoamK', 8.0, 0.12, 0.4, 0.85)
P.set('waterRiverFoamK2', 24.0, 8.0, 0.3, 0.8)
P.set('waterShoreK0', 64.0, 256.0, 192.0, 0.25)
P.set('waterShoreK1', 96.0, 6.0, 0.3, 0.85)
P.set('waterShoreK4', 35.0, 105.0, 64.0, 0.0)
P.set('stepU', STEP)
P.set('tclock', 7.5)
FULL = 0.85


def draw(red, mode):
    P.set('waterRiverFoamRed', red)
    P.set('mode', mode)
    return P.draw().astype(np.float64)


print('arm: shaders %s, waterRiverFoamRed %d' % (a.shaders, a.red))
fails = 0
solid = draw(32, 0)[..., 1]          # the solid law (paint): where the streaks' cores are
arm = draw(a.red, 0)[..., 1]
core = solid >= 0.8 * FULL
holes = float((arm[core] < 0.3 * FULL).mean()) if core.any() else 0.0
lab, n = ndimage.label(arm > 0.5 * FULL)
areas = ndimage.sum(np.ones_like(arm), lab, index=np.arange(1, n + 1)) if n else np.zeros(1)
biggest = float(areas.max()) * (STEP * 0.0142875) ** 2
ok = holes >= 0.25 and biggest <= 0.5
fails += not ok
print('LACE mean alpha %.4f (solid law %.4f); core pixels %d, holes in them %.1f %%; %d solid patches, the largest %.3f m^2  %s' % (
    arm.mean(), solid.mean(), int(core.sum()), 100 * holes, n, biggest, 'PASS' if ok else 'FAIL'))
b = draw(a.red, 1)
dry = b[..., 1] < 4.0
on_dry = int((b[..., 0][dry] > 0.01).sum())
wet_mean = float(b[..., 0][b[..., 1] > 32.0].mean())
ok = on_dry == 0 and wet_mean > 0.0
fails += not ok
print('LAND foam on the dry bank (column < 4 u): %d of %d samples > 0.01 (max %.3f); over > 32 u of water mean %.4f  %s' % (
    on_dry, int(dry.sum()), float(b[..., 0][dry].max()), wet_mean, 'PASS' if ok else 'FAIL'))
print('VERDICT %s' % ('PASS' if fails == 0 else 'FAIL (%d)' % fails))
