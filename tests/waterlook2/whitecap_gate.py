"""lane FIX1031B (WATERLOOK1, LATE ITEM 8 foam): whitecap cover on open sea vs the measured whitecap fraction.

bungo 10-31: "our foaming is missing? we need really good foaming". Step 1 (MEASURED, notes/fix1031b/water/HANDOFF.md
"## Foam + real values"): the old ramp 1 - smoothstep( 0.15, 0.55, J ) sat under the lowest Jacobian the spectrum makes
(p0.1 0.63 at 9.9 m/s), so no whitecap showed at any wind.
This gate runs the REAL statement `o.white = ...;` and wwOcWhite() cut from res/shaders/water_ocean.glsl through the driver
(glh.py) over the near sea's Jacobian distribution per wind (tests/waterlook2/whitecap_J.npz: WATERLOOK1's float64 ocean
model, all 3 sea cascades, 250000 samples a wind; notes/fix1031b/water/tools/whitecap_hist.py), ws = 1.
Reference: Monahan and O'Muircheartaigh 1980, W = 3.84e-6 U^3.41 (fraction of the sea surface whitecapped).
  cover   at U = 4, 5, 6, 8, 10, 12, 15 m/s the mean cover is within a factor 1.5 of W(U)
  calm    the cover at 4 m/s (NDBC 44013's Jun-Oct median wind 4.0-4.1 m/s, 2022-2023) is under 0.1 %
VERDICT PASS = both. Red --red 2048 = WW_CELL_WATER_OCEAN_RED=oldwhite (the ramp before) must FAIL.
python -I whitecap_gate.py [--shaders DIR] [--red N]
"""
import argparse
import os
import re
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import glh  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(glh.TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
a = ap.parse_args()

src = open(os.path.join(a.shaders, 'water_ocean.glsl'), encoding='latin-1').read()
stmt = re.search(r'\to\.white = [^;]*;', src)
fn = re.search(r'float wwOcWhite\( float J, float U \)\n\{.*?\n\}', src, re.S)
assert stmt, 'o.white statement not found'
print('the shader\'s statement:', ' '.join(stmt.group(0).split()))
D = np.load(os.path.join(HERE, 'whitecap_J.npz'))
US, edges, H = D['U'], D['edges'], D['H'].astype(np.float64)
centres = 0.5 * (edges[1:] + edges[:-1])
NB = len(centres)
FRAG = ('#version 410 core\nuniform int waterOceanRed;\nuniform float U;\nuniform sampler2D jtex;\n'
        + (fn.group(0) if fn else '') + r"""
struct Wh { float white; };
out vec4 oc;
void main()
{
	float J = texelFetch( jtex, ivec2( gl_FragCoord.xy ), 0 ).r;
	float ws = 1.0;
	Wh o;
""" + stmt.group(0) + r"""
	oc = vec4( o.white, 0.0, 0.0, 1.0 );
}
""")
W, Hh = 100, NB // 100
P = glh.Prog(FRAG, W, Hh)
jt = centres.astype(np.float32).reshape(Hh, W)
P.tex('jtex', jt)
P.set('waterOceanRed', a.red)
print('arm: shaders %s, waterOceanRed %d' % (a.shaders, a.red))
ok = True
for i, U in enumerate(US):
    P.set('U', float(U))
    white = P.draw()[..., 0].astype(np.float64).reshape(-1)
    cov = float((white * H[i]).sum() / H[i].sum())
    mon = 3.84e-6 * U ** 3.41
    r = cov / mon
    good = 1 / 1.5 <= r <= 1.5
    if U == 4.0:
        good &= cov < 0.001
    ok &= good
    print('  U %5.1f m/s: cover %.5f, Monahan %.5f, ratio %.3f %s' % (U, cov, mon, r, 'ok' if good else 'FAIL'))
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
