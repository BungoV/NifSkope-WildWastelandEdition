"""lane FIX1031B (WATERLOOK1 overhaul, LATE ITEM 7 gate): do the water's normals follow the baked flow direction?

Runs the REAL water_flowwaves.glsl (wwWaterOceanFlowed, wwWaterFlowSmooth, wwOcFlowNorm) and water_info.glsl (the flow
and body lookups, uploaded as one uniform tile: a river body, class 1, flow (0.5, 0.3) bake units = m/s, the numbers of
the Cutler Bend bake: 0.36-0.79) through the driver. The wave field itself is a stub (6 fixed sines, 20-90 u: the
spectrum's job is not under test here), read at the moved positions the flow law asks for. Two frames 0.5 s apart
(world clock 1.0 and 1.5 s, cellocean.cpp's 4 s period and weights: no restart between them); the pattern's shift is the
cross-correlation peak of the slope field (256 x 256 u at 1 u a sample).
  dir    cos( shift, baked flow ) > 0.9
  speed  |shift| within 30 % of |v| dt (v = 0.583 m/s = 40.8 u/s: 20.4 u in 0.5 s)
VERDICT PASS = both. Reds must FAIL: --red 1024 (WW_CELL_WATER_OCEAN_RED=rawflow, the reading before FIX1031B's first
agent), --red 256 (noflow). The shipped look (Ocean waves row off) draws no flowed normals at all.
python -I flow_gate.py [--shaders DIR] [--red N]
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

N = 256
VX, VY, WCNT = 0.5, 0.3, 64.0
STUB = r"""
uniform int waterOceanRed;
uniform vec4 waterOceanInvL[2];
uniform vec4 waterOceanWind;
const float kOcMetre = 0.0142875;
struct WwOcean { vec2 slope; float var; float w; float white; float cau; };
vec2 stubSlope( vec2 Q )
{
	const vec3 K[6] = vec3[6]( vec3( 0.0700, 0.0300, 0.3 ), vec3( -0.0400, 0.0800, 1.7 ), vec3( 0.1100, -0.0500, 2.9 ),
		vec3( 0.0250, 0.1300, 4.1 ), vec3( -0.0900, -0.0700, 5.3 ), vec3( 0.1500, 0.0900, 0.9 ) );
	vec2 s = vec2( 0.0 );
	for ( int i = 0; i < 6; i++ )
		s += K[i].xy * cos( dot( K[i].xy, Q ) + K[i].z );
	return s;
}
WwOcean wwWaterOceanQ( vec2 P, vec2 Q, vec2 dPx, vec2 dPy, out int setOut )
{
	setOut = 1;
	return WwOcean( stubSlope( Q ), 0.01, 1.0, 0.0, 1.0 );
}
WwOcean wwWaterOcean( vec2 P, vec2 dPx, vec2 dPy )
{
	int s;
	return wwWaterOceanQ( P, P, dPx, dPy, s );
}
"""
FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + STUB
        + glh.glsl(a.shaders, 'water_flowwaves.glsl') + r"""
out vec4 oc;
void main()
{
	vec2 P = vec2( 1000.0, 600.0 ) + gl_FragCoord.xy;
	WwOcean o = wwWaterOceanFlowed( P, vec2( 1.0, 0.0 ), vec2( 0.0, 1.0 ) );
	oc = vec4( o.slope, 0.0, 1.0 );
}
""")
P = glh.Prog(FRAG, N, N)
word = 1 | (255 << 16) | (40 << 24)
P.tex('waterInfoIndex', np.array([[[0, word]]], np.uint32), 'u')
P.tex('waterInfoAtlas', np.zeros((1, 1), np.uint32), 'u')
bodies = np.zeros((1, 256, 4), np.float32)
bodies[0, 1] = (1300.0, 1.0, 0.0, 1000.0)
P.tex('waterInfoBodies', bodies)
fw = (int(round(VX * WCNT)) & 0xFFFF) | ((int(round(VY * WCNT)) & 0xFFFF) << 16)
P.tex('waterFlowIndex', np.array([[[0, fw]]], np.uint32), 'u')
P.tex('waterFlowAtlas', np.zeros((1, 1), np.uint32), 'u')
P.set('waterInfoGrid', 0.0, 0.0, 128.0, 32.0)
P.set('waterInfoLayout', 1.0, 1.0, 16.0, 1.0)
P.set('waterInfoOn', 1)
P.set('waterFlowGrid', 0.0, 0.0, 128.0, WCNT)
P.set('waterFlowLayout', 1.0, 1.0, 16.0, 1.0)
P.set('waterFlowOn', 1)
P.set4v('waterOceanInvL', [(0.01, 0.1, 0.5, 1.0), (0.01, 0.1, 0.5, 1.0)])
P.set('waterOceanWind', 1.0, 0.0, 5.0, 3.61)
P.set4v('waterOceanFlowCorr', [(0.5, 0.5, 0.5, 0.5)] * 16)
P.set('waterOceanRed', a.red)


def frame(t):
    T = 4.0
    fA = math.fmod(t, T) / T
    wa = 1.0 - abs(2.0 * fA - 1.0)
    P.set('waterOceanFlow', fA * T, math.fmod(fA + 0.5, 1.0) * T, wa, 1.0 - wa)
    return P.draw()[..., 0].astype(np.float64)


f0, f1 = frame(1.0), frame(1.5)
win = np.outer(np.hanning(N), np.hanning(N))
A = np.fft.fft2((f0 - f0.mean()) * win)
B = np.fft.fft2((f1 - f1.mean()) * win)
xc = np.real(np.fft.ifft2(np.conj(A) * B))
iy, ix = np.unravel_index(np.argmax(xc), xc.shape)
sy = iy - N if iy > N // 2 else iy
sx = ix - N if ix > N // 2 else ix
vb = np.array([VX, VY]) / 0.0142875 if (a.red & 1024) == 0 else np.array([VX, VY])
expect = np.linalg.norm(vb) * 0.5
shift = np.array([sx, sy], float)
mag = float(np.linalg.norm(shift))
cosv = float(shift @ vb / (mag * np.linalg.norm(vb))) if mag > 0 else 0.0
want = np.array([VX, VY]) / 0.0142875 * 0.5
print('arm: shaders %s, waterOceanRed %d' % (a.shaders, a.red))
print('baked flow (%.3f, %.3f) m/s = (%.1f, %.1f) u/s; expected shift in 0.5 s (%.1f, %.1f) u' % (
    VX, VY, *(want * 2), *want))
print('measured shift (%d, %d) u: |shift| %.1f u (want %.1f), cos to the flow %.3f' % (sx, sy, mag, np.linalg.norm(want),
                                                                                        cosv))
good = cosv > 0.9 and abs(mag / np.linalg.norm(want) - 1.0) <= 0.3
print('VERDICT %s' % ('PASS' if good else 'FAIL'))
sys.exit(0 if good else 1)
