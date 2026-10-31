"""lane FIX1031B (WATERLOOK1, LATE ITEM 8 foam): contact foam around objects standing in a river (boats, piers, rocks).

Step 1 (MEASURED): the Contact foam row shipped OFF, and its strength came from the wind waves alone: a river's 40 m
fetch gives Hs 0.032 m, amount (Hs / 0.3)^2.5 = 0.001 at Cutler Bend: the boats got no foam even with the row on.
This gate runs the REAL water_contact.glsl (wwWaterContact) with water_info.glsl + water_shore.glsl through the driver;
the contact distance comes from a stub of water_ripple.glsl's wwContactDist (d = the pixel column x 2 u, trust 1, foot 1 u:
the depth-buffer search is not under test). One uniform river tile (body 1, class 1, 2.9 m deep, shore steps 255 = open
water, flow (0.5, 0.3) bake units = 0.583 m/s), the uniforms cellwater.cpp sets (K 4 / 2 / 0.04 / 0.9, shore K2 wind
5 m/s, swell 0.264, fetch 40 m, full foam 0.3 m; K3 0.25 / 0.78 / 0.35 / 0.05), 32 clock times 0..8 s.
  ring    mean density at the object (d < 0.5 m) >= 0.15 at 0.583 m/s; <= 0.03 at 3 m out (a ~1 m band)
  slow    at 0.1 m/s (slack water) the density at the object <= 0.03
VERDICT PASS = all. Reds must FAIL: --off (the Contact foam row off: the shipped default) and --red 16
(WW_CELL_WATER_CONTACT_RED=noflow: the wind-only law before LATE ITEM 8).
python -I contactfoam_gate.py [--shaders DIR] [--red N] [--off]
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

STUB = r"""
float wwContactDist( float vert, vec3 P, vec3 S, vec3 eye, out vec2 dir, out float foot, out float trust )
{
	dir = vec2( 1.0, 0.0 );
	foot = 1.0;
	trust = 1.0;
	return vert > 1e8 ? 1e9 : P.z;	// the stub's contact distance rides in P.z
}
"""
FRAG = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_shore.glsl')
        + STUB + glh.glsl(a.shaders, 'water_contact.glsl') + r"""
out vec4 oc;
void main()
{
	float d = floor( gl_FragCoord.x ) * 2.0;
	float t = floor( gl_FragCoord.y ) * 0.25;
	vec3 P = vec3( 1000.0 + 0.37 * gl_FragCoord.y, 600.0, d );
	oc = wwWaterContact( 100.0, P, vec3( 0.0 ), vec3( 0.0 ), t );
}
""")
NX, NT = 256, 32
P = glh.Prog(FRAG, NX, NT)
WCNT = 64.0


def setup(vx, vy):
    word = 1 | (255 << 16) | (40 << 24)
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
    P.set('waterContactOn', 0 if a.off else 1)
    P.set('waterContactRed', a.red)
    P.set('waterContactK', 4.0, 2.0, 0.04, 0.9)
    P.set('waterShoreOn', 3)
    P.set('waterShoreRed', 0)
    P.set('waterShoreK0', 64.0, 256.0, 192.0, 0.25)
    P.set('waterShoreK1', 96.0, 6.0, 0.3, 0.85)
    P.set('waterShoreLaw', 1)
    P.set('waterShoreK2', 5.0, 0.264, 40.0, 0.3)
    P.set('waterShoreK3', 0.25, 0.78, 0.35, 0.05)
    P.set('waterShoreEdgeLaw', 1)
    P.set('waterShoreK4', 35.0, 105.0, 64.0, 0.0)


d = np.arange(NX) * 2.0
near, far = d < 35.0, (d >= 200.0) & (d < 230.0)   # < 0.5 m; ~3 m out
print('arm: shaders %s, waterContactRed %d%s' % (a.shaders, a.red, ', row OFF' if a.off else ''))
setup(0.5, 0.3)
f = P.draw().astype(np.float64)[..., 0].mean(0)
setup(0.06, 0.08)
s = P.draw().astype(np.float64)[..., 0].mean(0)
ring_ok = f[near].mean() >= 0.15 and f[far].mean() <= 0.03
slow_ok = s[near].mean() <= 0.03
for lo, hi in ((0, 35), (35, 70), (70, 140), (140, 210), (210, 350), (350, 512)):
    m = (d >= lo) & (d < hi)
    print('  %3d-%3d u (%.1f-%.1f m): density 0.583 m/s %.4f, 0.1 m/s %.4f' % (lo, hi, lo * 0.0142875, hi * 0.0142875,
                                                                          f[m].mean(), s[m].mean()))
print('ring: at the object %.4f (>= 0.15), 3 m out %.4f (<= 0.03): %s' % (f[near].mean(), f[far].mean(),
                                                                         'ok' if ring_ok else 'FAIL'))
print('slow: at the object %.4f (<= 0.03): %s' % (s[near].mean(), 'ok' if slow_ok else 'FAIL'))
good = ring_ok and slow_ok
print('VERDICT %s' % ('PASS' if good else 'FAIL'))
sys.exit(0 if good else 1)
