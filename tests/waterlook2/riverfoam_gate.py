"""lane FIX1031B (WATERLOOK1, LATE ITEM 8 foam): river foam from the flow's speed and depth, carried at the river's speed.

Step 1 (MEASURED): the River foam row shipped OFF, and its law read the bake's flow as u/s (it is m/s since the first
FIX1031B agent): the streaks crawled at 1/70 of the river (0.4-0.8 u/s) with a 120 s life.
This gate runs the REAL water_riverfoam.glsl (wwWaterRiverFoam) with water_info.glsl + water_shore.glsl (its noise)
through the driver over one uniform river tile (body 1, class 1, flow (0.5, 0.3) bake units = 0.583 m/s, Cutler Bend's
0.36-0.79), the uniforms cellwater.cpp sets (K = life 8 s / 0.12 / 0.4 m/s / 0.85; K2 24 / 8 / 0.3 / 0.8; raw red: life
120 s), 256 x 256 u at 1 u a sample, foot 1 u.
  cover   mean alpha at 0.583 m/s over a 2.9 m deep bed in [0.03, 0.15]; at 0.29 m/s lower than at 0.58 m/s (the source
          grows with speed); over a 0.5 m deep bed >= 1.5 x the deep bed's (shallows foam more)
  carry   the foam pattern's shift between world clock 1.0 and 1.5 s is along the flow (cos > 0.9) and within 30 % of
          v dt (20.4 u); read with the iso bit (round blobs, the same carry code)
VERDICT PASS = both. Reds must FAIL: --off (the River foam row off: the shipped default before LATE ITEM 8) and
--red 16 (WW_CELL_WATER_RIVERFOAM_RED=raw: the raw flow reading, 120 s life).
python -I riverfoam_gate.py [--shaders DIR] [--red N] [--off]
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
ap.add_argument('--off', action='store_true')
a = ap.parse_args()

N = 256
WCNT = 64.0
FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_shore.glsl')
        + '\n' + glh.glsl(a.shaders, 'water_riverfoam.glsl') + r"""
uniform float tclock;
out vec4 oc;
void main()
{
	vec2 P = vec2( 1000.0, 600.0 ) + gl_FragCoord.xy;
	oc = wwWaterRiverFoam( P, tclock, 1.0 );
}
""")
P = glh.Prog(FRAG, N, N)


def setup(vx, vy, depth_code):
    word = 1 | (255 << 16) | (depth_code << 24)
    P.tex('waterInfoIndex', np.array([[[0, word]]], np.uint32), 'u')
    P.tex('waterInfoAtlas', np.zeros((1, 1), np.uint32), 'u')
    bodies = np.zeros((1, 256, 4), np.float32)
    bodies[0, 1] = (1300.0, 1.0, 0.0, 1000.0)
    P.tex('waterInfoBodies', bodies)
    fw = (int(round(vx * WCNT)) & 0xFFFF) | ((int(round(vy * WCNT)) & 0xFFFF) << 16)
    P.tex('waterFlowIndex', np.array([[[0, fw]]], np.uint32), 'u')
    P.tex('waterFlowAtlas', np.zeros((1, 1), np.uint32), 'u')
    P.set('waterInfoGrid', 0.0, 0.0, 128.0, 32.0)
    P.set('waterInfoLayout', 1.0, 1.0, 16.0, 1.0)
    P.set('waterInfoOn', 1)
    P.set('waterFlowGrid', 0.0, 0.0, 128.0, WCNT)
    P.set('waterFlowLayout', 1.0, 1.0, 16.0, 1.0)
    P.set('waterFlowOn', 1)
    P.set('waterRiverFoamOn', 0 if a.off else 1)
    P.set('waterRiverFoamRed', a.red)
    P.set('waterRiverFoamK', 120.0 if (a.red & 16) else 8.0, 0.12, 0.4, 0.85)
    P.set('waterRiverFoamK2', 24.0, 8.0, 0.3, 0.8)


def draw(t):
    P.set('tclock', t)
    return P.draw().astype(np.float64)


DEEP, SHALLOW = 40, 13          # depth codes: (c / 255)^2 x 8192 u = 201 u (2.9 m), 21 u (0.30 m)
dm = lambda c: (c / 255.0) ** 2 * 8192.0 * 0.0142875
print('arm: shaders %s, waterRiverFoamRed %d%s' % (a.shaders, a.red, ', row OFF' if a.off else ''))
res = {}
for name, vx, vy, code in (('fast deep', 0.5, 0.3, DEEP), ('slow deep', 0.25, 0.15, DEEP), ('fast shallow', 0.5, 0.3, SHALLOW)):
    setup(vx, vy, code)
    covs = [draw(t)[..., 1].mean() for t in (0.3, 1.1, 2.7, 4.9)]
    res[name] = float(np.mean(covs))
    f = draw(1.0)
    print('  %-12s v %.3f m/s, bed %.2f m: density D %.4f, mean alpha %.4f' % (
        name, np.hypot(vx, vy), dm(code), f[..., 0].mean(), res[name]))
cover_ok = 0.03 <= res['fast deep'] <= 0.15 and res['slow deep'] < res['fast deep'] and \
    res['fast shallow'] >= 1.5 * res['fast deep']
print('cover: fast deep %.4f in [0.03, 0.15], slow %.4f < fast, shallow %.4f >= 1.5 x deep: %s' % (
    res['fast deep'], res['slow deep'], res['fast shallow'], 'ok' if cover_ok else 'FAIL'))

setup(0.5, 0.3, DEEP)
# the carry is read with the iso bit (4: round 24 u blobs, the same carry code): 192 u streaks along the flow leave the
# cross-correlation flat along the very axis the shift runs on
P.set('waterRiverFoamRed', a.red | 4)
f0, f1 = draw(1.0)[..., 1], draw(1.5)[..., 1]
win = np.outer(np.hanning(N), np.hanning(N))
A = np.fft.fft2((f0 - f0.mean()) * win)
B = np.fft.fft2((f1 - f1.mean()) * win)
xc = np.real(np.fft.ifft2(np.conj(A) * B))
iy, ix = np.unravel_index(np.argmax(xc), xc.shape)
sy = iy - N if iy > N // 2 else iy
sx = ix - N if ix > N // 2 else ix
want = np.array([0.5, 0.3]) / 0.0142875 * 0.5
shift = np.array([sx, sy], float)
mag = float(np.linalg.norm(shift))
cosv = float(shift @ want / (mag * np.linalg.norm(want))) if mag > 0 else 0.0
carry_ok = cosv > 0.9 and abs(mag / np.linalg.norm(want) - 1.0) <= 0.3
print('carry: shift (%d, %d) u in 0.5 s, |shift| %.1f u (want %.1f), cos %.3f: %s' % (
    sx, sy, mag, np.linalg.norm(want), cosv, 'ok' if carry_ok else 'FAIL'))
good = cover_ok and carry_ok
print('VERDICT %s' % ('PASS' if good else 'FAIL'))
sys.exit(0 if good else 1)
