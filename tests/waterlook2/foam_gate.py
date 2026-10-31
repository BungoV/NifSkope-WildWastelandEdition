"""lane FIX1031B (WATERLOOK1 overhaul, LATE ITEM 7 gate): a foam band from the baked shore distance, none on calm deep water.

The REAL water_shore.glsl (wwShoreField, wwShoreWave, wwShoreFoamWave with its lace detail) and water_info.glsl run
through the driver over a baked sea beach uploaded as .lodl water-info tiles (128 u texels, body 1 = sea, class 0,
area 1e6 texels; the waterline at x = 1024 u, the bed 1:30 = depth (x - 1024) / 30 u, shore steps 32 u, depth code
sqrt( depth / 8192 ) x 255). The water column behind = the same bed depth. The uniforms are cellwater.cpp's (K0 64 / 256 /
192 / 0.25, K1 96 / 6 / 0.3 / 0.85, K3 0.25 / 0.78 / 0.35 / 0.05, contact K4 35 / 105 / 64), the wind 5 m/s and its
swell 0.264 m (foamDrive for a weather byte of 0.21). Coverage = the foam's alpha x opacity K1.w, the share main mixes
in, averaged over 64 positions along the shore x 64 times of the world clock (0..12 s).
  band   mean coverage over the first 600 u of water >= 0.04 (the breakers of a 0.29 m sea break under 0.37 m)
  calm   max coverage past 1600 u (the bed 0.76 m+ down, twice the breaking depth) <= 0.005
VERDICT PASS = both. Reds must FAIL: --red 1 = the Shore foam row off (the shipped default before FIX1031B), and
--fullfoam 0.6 = WW_CELL_WATER_FOAM_RED=dim (full foam only at 0.6 m breakers, the calibration before: 1 % cover).
python -I foam_gate.py [--shaders DIR] [--red N] [--fullfoam M]
"""
import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import glh  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(glh.TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
ap.add_argument('--fullfoam', type=float, default=0.0, help='K2.w; 0 = read from the tree\'s src/gl/cellwater.cpp')
a = ap.parse_args()
if a.fullfoam <= 0.0:   # wwCellWaterSurfConsts' full-foam breaker height, the shipping value (not the red's)
    import re
    src = open(os.path.join(glh.TREE, 'src', 'gl', 'cellwater.cpp'), encoding='latin-1').read()
    m = re.search(r'40\.0f, st\(\)\.foamDim \? [0-9.]+f : ([0-9.]+)f', src) or re.search(r'40\.0f, ([0-9.]+)f \}', src)
    a.fullfoam = float(m.group(1))

TX, E, NTX, NTY = 128.0, 16, 4, 1
WL, SLOPE = 1024.0, 30.0
words = np.zeros((NTY * E, NTX * E), np.uint32)
for ty in range(NTY * E):
    for tx in range(NTX * E):
        xc = (tx + 0.5) * TX
        if xc < WL:
            continue
        d = xc - WL
        dep = d / SLOPE
        steps = min(255, int(round(d / 32.0)))
        code = min(255, int(round(np.sqrt(dep / 8192.0) * 255.0)))
        words[ty, tx] = 1 | (steps << 16) | (code << 24)
index = np.zeros((NTY, NTX, 2), np.uint32)
atlas = np.zeros((E * NTY, E * NTX), np.uint32)   # 4 tiles a row (waterInfoLayout.w)
for k in range(NTX):
    index[0, k] = (k + 1, 0)
    atlas[0:E, k * E:(k + 1) * E] = words[0:E, k * E:(k + 1) * E]

FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_shore.glsl')
        + r"""
uniform float slope, wl;
out vec4 oc;
void main()
{
	int i = int( gl_FragCoord.x ), j = int( gl_FragCoord.y );
	float d = float( i ) * 8.0 + 4.0;
	vec2 P = vec2( wl + d, 192.0 + 25.3 * float( j ) + 13.0 * fract( float( j ) * 0.618 ) );	// inside the map's 2048 u
	float t = 0.19 * float( j );
	vec2 field = wwShoreField( P );
	vec3 dbg;
	float col = d / slope;
	float f = ( waterShoreOn & 2 ) == 0 ? 0.0 : wwShoreFoamWave( field, P, t, col, 0.0, 2.0, dbg );
	oc = vec4( f * waterShoreK1.w, field.x, dbg.x, 1.0 );
}
""")
P = glh.Prog(FRAG, 256, 64)
P.tex('waterInfoIndex', index, 'u')
P.tex('waterInfoAtlas', atlas, 'u')
bodies = np.zeros((1, 256, 4), np.float32)
bodies[0, 1] = (0.0, 0.0, 0.0, 1.0e6)
P.tex('waterInfoBodies', bodies)
P.set('waterInfoGrid', 0.0, 0.0, TX, 32.0)
P.set('waterInfoLayout', float(NTX), float(NTY), float(E), float(NTX))
P.set('waterInfoOn', 1)
P.set('slope', SLOPE)
P.set('wl', WL)
P.set('waterShoreOn', 1 if a.red == 1 else 3)
P.set('waterShoreRed', 0)
P.set('waterShoreK0', 64.0, 256.0, 192.0, 0.25)
P.set('waterShoreK1', 96.0, 6.0, 0.3, 0.85)
P.set('waterShoreLaw', 1)
P.set('waterShoreK2', 5.0, 0.264, 40.0, a.fullfoam)
P.set('waterShoreK3', 0.25, 0.78, 0.35, 0.05)
P.set('waterShoreEdgeLaw', 1)
P.set('waterShoreK4', 35.0, 105.0, 64.0, 0.0)
out = P.draw().astype(np.float64)
cov = out[..., 0].mean(0)
d = np.arange(256) * 8.0 + 4.0
print('arm: shaders %s, %s, full foam at %.2f m breakers' % (
    a.shaders, 'Shore foam row OFF (red)' if a.red == 1 else 'Shore foam row ON', a.fullfoam))
print('Hs at the shore %.1f u (%.2f m); field at d=4 u %.0f, d=1000 u %.0f' % (
    out[0, 0, 2] * 64.0, out[0, 0, 2] * 64.0 * 0.0142875, out[0, 0, 1], out[0, 125, 1]))
for lo, hi in ((0, 200), (200, 400), (400, 600), (600, 800), (800, 1200), (1200, 1600), (1600, 2048)):
    m = (d >= lo) & (d < hi)
    print('  %4d-%4d u from the waterline: coverage mean %.4f max %.4f' % (lo, hi, cov[m].mean(), out[:, m, 0].max()))
band = float(cov[d <= 600].mean())
calm = float(out[:, d > 1600, 0].max())
good = band >= 0.04 and calm <= 0.005
print('band (0-600 u) %.4f (>= 0.04), calm (> 1600 u) max %.4f (<= 0.005)' % (band, calm))
print('VERDICT %s' % ('PASS' if good else 'FAIL'))
sys.exit(0 if good else 1)
