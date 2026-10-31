"""lane FIX1031B (WATERLOOK1 overhaul, LATE ITEM 5.1): does the water reflect the sky by Fresnel with the clear colour on?

bungo 10-31 (bungo_river_noreflect.webp): with the looks on (clear colour ON, Water reflections row OFF) the river showed
no reflection, not even the sky at grazing angles. Cause (REASONED from fo4_water.frag main, MEASURED below): with the
row off the surface mixed body and sky by the WATR's F (Schlick from the record's F0) x reflectivity (0.21 Charles,
0.29 ocean), a cap the game's opaque body hid but a see-through body cannot.
This runs the REAL functions (water_refl.glsl wwWaterReflPhys / wwWaterFresnel / wwWaterReflMix, with water_info /
water_depth / water_clear for the uniforms) through the driver: body 0, environment 1, so the output IS the reflected
share. The non-physical branch is fo4_water.frag main's two lines `mix( silted, sky, F Var.y )` (transcribed).
  share at incidence 0, 60, 80 deg (smooth water, alpha = the WATR sun power's 0.013) vs Schlick n 1.33
  (F0 = (0.33 / 2.33)^2 = 0.0201) within 3 %, for both Boston records (ExtOceanWater F0 0.058 refl 0.2935,
  ExtRiverCharlesUpper F0 0.1486 refl 0.2101), in bungo's case (clear ON, reflections row OFF) and the new default
  (both ON).
VERDICT PASS = all 12 within 3 %. --red 4 = WW_CELL_WATER_CLEAR_RED=old (today's clear colour) must FAIL.

python -I fresnel_gate.py [--shaders DIR] [--red N]
"""
import argparse
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import glh  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--shaders', default=os.path.join(glh.TREE, 'res', 'shaders'))
ap.add_argument('--red', type=int, default=0)
a = ap.parse_args()

ANG = (0.0, 60.0, 80.0)
REC = (('ExtOceanWater', 0.058, 0.2935, 2820.0), ('ExtRiverCharlesUpper', 0.1486, 0.2101, 1901.0))
FRAG = ('#version 410 core\n#define WW_WATER_CLEAR 1\nuniform vec4 waterSky[3];\n'
        + glh.glsl(a.shaders, 'water_info.glsl') + '\n' + glh.glsl(a.shaders, 'water_depth.glsl') + '\n'
        + glh.glsl(a.shaders, 'water_clear.glsl') + '\n' + glh.glsl(a.shaders, 'water_refl.glsl') + r"""
uniform vec3 ang;
uniform vec4 recF0, recRefl, recPow;
out vec4 oc;
void main()
{
	int ia = int( gl_FragCoord.x ), ir = int( gl_FragCoord.y );
	float th = radians( ang[ia] );
	float NoV = cos( th );
	float F0g = recF0[ir], refl = recRefl[ir];
	float alpha = sqrt( 2.0 / ( 4.0 * recPow[ir] + 2.0 ) );	// wwWaterAlpha2's a0 from the sun spec power
	float c = 1.0 - NoV;
	float Fg = F0g + ( 1.0 - F0g ) * c * c * c * c * c;	// fo4_water.frag main's F
	float Fp = wwWaterFresnel( NoV, alpha, waterGlintK.y );
	vec3 body = vec3( 0.0 ), env = vec3( 1.0 ), sky = vec3( 1.0 ), far = vec3( 0.0 );
	vec3 lin = wwWaterReflPhys() ? wwWaterReflMix( body, env, Fp, Fg, refl, far, 1.0 ) : mix( body, sky, Fg * refl );
	oc = vec4( lin.g, wwWaterReflPhys() ? 1.0 : 0.0, 0.0, 1.0 );
}
""")

P = glh.Prog(FRAG, 3, 2)
P.set('ang', *ANG)
P.set('recF0', REC[0][1], REC[1][1], 0.0, 0.0)
P.set('recRefl', REC[0][2], REC[1][2], 0.0, 0.0)
P.set('recPow', REC[0][3], REC[1][3], 0.0, 0.0)
P.set('waterGlintK', 16.0, 0.0204, 0.18, 0.015)   # cellwater.cpp's values
P.set('waterReflRed', 0)


def schlick(th):
    f0 = (0.33 / 2.33) ** 2
    return f0 + (1 - f0) * (1 - math.cos(math.radians(th))) ** 5


print('arm: shaders %s, waterClearRed %d' % (a.shaders, a.red))
ok = True
for case, reflOn in (('bungo: clear ON, reflections row OFF', 0), ('default: clear ON, reflections ON', 1)):
    P.set('waterClearOn', 1)
    P.set('waterClearRed', a.red)
    P.set('waterReflOn', reflOn)
    out = P.draw()
    print(case)
    for ir, rec in enumerate(REC):
        for ia, th in enumerate(ANG):
            s, ref = float(out[ir, ia, 0]), schlick(th)
            err = s / ref - 1.0
            good = abs(err) <= 0.03
            ok &= good
            print('  %-22s %4.0f deg  share %.4f  Schlick n1.33 %.4f  %+6.1f %%  phys %d  %s' % (
                rec[0], th, s, ref, 100 * err, int(out[ir, ia, 1]), 'ok' if good else 'OFF'))
print('VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
