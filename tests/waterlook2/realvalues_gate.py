"""lane FIX1031B (WATERLOOK1, LATE ITEMs 9-10): the clear colour per depth step vs Boston's MEASURED water.

Sources (notes/fix1031b/water/REALVALUES.md, every number cited there with station and dates):
  sea   (ExtOceanWater, spot 5 = the outer harbour / Broad Sound shore): MWRA report 2018-06 Fig. 15, harbour-wide mean
        1 % light depth 7.3 m over 1996-2017 -> Kd(PAR) = ln(100) / 7.3 = 0.631 /m (the centre); band = Luis et al.
        2019 in-situ Secchi at MWRA station 142 (Deer I. - Lovells I.), 1-5 m over 2013-2017 -> Kd 0.34 .. 1.70 /m.
  river (ExtRiverCharlesUpper, spot 6): EPA Region 1 Clean Charles 2002, mean Secchi 1.0 m Daly Field - BU Bridge
        (the centre: the reach nearest the upper river) and 1.5 m Esplanade - New Charles River Dam -> band 1.0 .. 1.5 m.
  lake  (ExtLakeWater): no cited Commonwealth value found: not gated here (kept at the overhaul's 2.0 m).
Bio-optical rule (stated): k(550) = Kd(PAR) where an irradiance profile was measured, else Poole & Atkins 1929
Kd = 1.7 / Secchi; spectral shape = pure water (Pope & Fry 1997) + non-water absorption exp(-0.011 (l - 550)) + particle
backscatter ~ 550 / l, sized so the deep reflectance R(550) = 0.33 bb / k = 0.040 (tools/boston_coef.py; the shader's
own law). This gate re-derives k from the cited values in Python, renders the REAL water_clear.glsl with those k
substituted (the reference arms) and with the tree's own constants, through the driver (glh.py), same light and display
as colour_gate.py (45 deg down, sun 33.5 deg, wet bed albedo 0.25, dry sand in air = 178 levels).
Gates per class:
  band    every depth step's display RGB lies inside the per-channel envelope of 9 renders log-spaced over the cited
          k range (+-1 level; colour is not monotone in k per channel, so the two ends alone are no band)
  centre  every depth step within 2 display levels (per channel) of the cited centre's render: the constants ARE the
          measured values, not merely inside a wide range
VERDICT PASS = all. --red 5 = WW_CELL_WATER_CLEAR_RED=secchi (the overhaul's first constants, sea 2.5 m, river 1.2 m)
must FAIL; --red 4 (old clear ocean) must FAIL.
python -I realvalues_gate.py [--shaders DIR] [--red N]
"""
import argparse
import math
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

AW = (0.340, 0.0565, 0.00922)
BW = (0.0004, 0.00095, 0.00225)


def coef(kg, rg=0.040, slope=0.011):
    ashape = [math.exp(-slope * (l - 550)) for l in (650, 550, 450)]
    bshape = [550.0 / l for l in (650, 550, 450)]
    bbg = rg * kg / 0.33
    fb = (bbg - BW[1]) / bshape[1]
    fa = (kg - AW[1] - bbg) / ashape[1]
    k = [AW[i] + fa * ashape[i] + BW[i] + fb * bshape[i] for i in range(3)]
    R = [0.33 * (BW[i] + fb * bshape[i]) / k[i] for i in range(3)]
    return k, R


LN100 = math.log(100.0)
# class -> (centre k550, band k550 lo, hi, label)
CITED = {0: (LN100 / 7.3, 1.7 / 5.0, 1.7 / 1.0, 'sea: MWRA Kd(PAR) 0.631; band Secchi 1-5 m (st. 142)'),
         1: (1.7 / 1.0, 1.7 / 1.5, 1.7 / 1.0, 'river: Charles Secchi 1.0 m; band 1.0-1.5 m (EPA 2002)')}
DEPTHS = (0.25, 0.5, 1.0, 2.0, 3.0, 5.0, 10.0, -1.0)
CASES = (('sea', 0, (46, 61, 57)), ('river', 1, (47, 51, 43)))
SUN, SKY, ELEV, ALB = (2.5, 2.4, 2.2), (0.35, 0.45, 0.60), 33.5, 0.25

SRC = glh.glsl(a.shaders, 'water_clear.glsl')
RE_K = re.compile(r'const vec3 kWwBostonK\[3\] = [^;]*;')
RE_R = re.compile(r'const vec3 kWwBostonR\[3\] = [^;]*;', re.S)
assert len(RE_K.findall(SRC)) == 1 and len(RE_R.findall(SRC)) == 1, 'constants not found once in water_clear.glsl'


def subst(src, ks, rs):
    v = lambda t: 'vec3( %.6f, %.6f, %.6f )' % tuple(t)
    src = RE_K.sub('const vec3 kWwBostonK[3] = vec3[3]( %s, %s, %s );' % tuple(v(x) for x in ks), src)
    return RE_R.sub('const vec3 kWwBostonR[3] = vec3[3]( %s, %s, %s );' % tuple(v(x) for x in rs), src)


def render(clear_src, red):
    frag = ('#version 410 core\n' + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_depth.glsl')
            + '\n' + clear_src + r"""
uniform vec4 depths[2];
uniform vec4 deepCol[2];
uniform ivec2 cls;
uniform vec3 sunC, skyC;
uniform float sunCos, alb, mu;
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
	vec3 bedLit = alb * ( sunC * sunCos + skyC );
	float vert = zm < 0.0 ? 1e9 : zm / 0.0142875, along = zm < 0.0 ? 1e9 : vert / mu;
	oc = vec4( wwWaterClear( bedLit, along, vert ), 1.0 );
}
""")
    P = glh.Prog(frag, 8, 2)
    P.set4v('depths', [DEPTHS[0:4], DEPTHS[4:8]])
    P.set4v('deepCol', [tuple((c / 255.0) ** 2.2 for c in d) + (0.0,) for _, _, d in CASES])
    P.set('sunC', *SUN)
    P.set('skyC', *SKY)
    P.set('sunCos', math.sin(math.radians(ELEV)))
    P.set('alb', ALB)
    P.set('mu', math.cos(math.radians(45.0)))
    P.set('waterClearRed', red)
    iv = glh._fn('glUniform2i', None, glh.GLint, glh.GLint, glh.GLint)
    iv(P.loc('cls'), 0, 1)
    full = P.draw()[..., :3].astype(np.float64)
    sand = ALB * (np.array(SUN) * math.sin(math.radians(ELEV)) + np.array(SKY))
    return glh.srgb(full * (0.45 / sand.max())).astype(np.float64)


def arm(t):   # t: 'centre' or a fraction 0..1 along the band in log k
    ks, rs = [], []
    for c in (0, 1, 2):
        if c not in CITED:
            kg = 0.85
        elif t == 'centre':
            kg = CITED[c][0]
        else:
            kg = CITED[c][1] * (CITED[c][2] / CITED[c][1]) ** t
        k, R = coef(kg, 0.035 if c == 2 else 0.040)
        ks.append(k)
        rs.append(R)
    return render(subst(SRC, ks, rs), 0)


tree = render(SRC, a.red)
cen = arm('centre')
env = np.stack([arm(i / 8.0) for i in range(9)])   # the band's envelope: 9 k values log-spaced over the cited range
lo, hi = env.min(axis=0), env.max(axis=0)
print('arm: shaders %s, waterClearRed %d' % (a.shaders, a.red))
ok = True
for ic, (name, c, _) in enumerate(CASES):
    print('%s (%s)' % (name, CITED[c][3]))
    for idp, z in enumerate(DEPTHS):
        t, m = tree[ic, idp], cen[ic, idp]
        bmin, bmax = np.minimum(lo[ic, idp], hi[ic, idp]), np.maximum(lo[ic, idp], hi[ic, idp])
        inband = bool(np.all(t >= bmin - 1) and np.all(t <= bmax + 1))
        dc = float(np.abs(t - m).max())
        bad = (not inband) or dc > 2.0
        ok &= not bad
        print('  %-6s tree (%3d %3d %3d) centre (%3d %3d %3d) band (%3d %3d %3d)..(%3d %3d %3d) |d centre| %5.1f %s%s' % (
            'deep' if z < 0 else '%.2fm' % z, *t.round().astype(int), *m.round().astype(int),
            *bmin.round().astype(int), *bmax.round().astype(int), dc,
            'in band' if inband else 'OUT OF BAND', '' if dc <= 2.0 else ' OFF CENTRE'))
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
