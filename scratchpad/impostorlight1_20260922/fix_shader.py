# IMPOSTORLIGHT1 -- write the card lighting FIX into a copy of impostor_oct.frag.
#   python fix_shader.py <src frag> <dst frag> [lambert|oren]
# The replaced span is the old light loop, impostor_oct.frag:395-412 on
# bf6aa749 (sha1 198d305a), asserted exactly once. The diag anchor
# `\tvec3 lc = colour.rgb * ( lit + ambient );` is left as it was, so the
# ladder instrument still reads the fixed shader.
import sys
TAB = chr(9)
NLc = chr(10)

src, dst = sys.argv[1], sys.argv[2]
mode = sys.argv[3] if len(sys.argv) > 3 else 'oren'
s = open(src, 'rb').read().decode('utf-8')

OLD_START = '\t/* THE LIGHT TERMS ARE THE MESH PATH\'S, NOT THE RAW UNIFORMS (lane\n'
OLD_END = '\t\tlit += sqrt( max( lightSourceDiffuse[i].rgb, vec3( 0.0 ) ) ) * ndl;\n\t}\n'
a = s.find(OLD_START); b = s.find(OLD_END)
assert s.count(OLD_START) == 1 and s.count(OLD_END) == 1 and 0 <= a < b, 'anchors'
b += len(OLD_END)

diffuse_expr = {
    'lambert': '\t\tfloat diff = max( NdotL, 0.0 );\n',
    'oren': ('\t\tfloat diff = OrenNayarFull( NdotL, dot( nView, V ), dot( L, V ), 1.0 - smoothness )\n'
             '\t\t             * ( 1.0 - fresnelSchlick5( max( dot( V, normalize( L + V ) ), FLT_EPSILON ), 0.04 ) );\n'),
}[mode]

NEW = '''	/* THE LIGHT TERMS ARE THE MESH PATH'S, NOT THE RAW UNIFORMS (lane
	 * IMPOSTORLOOK1, 2026-09-19). `fo4_default.vert` hands its fragment stage
	 * `A = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale )` and
	 * `D = vec4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale )` -- the
	 * square roots are the FO4 path's gamma convention and the 0.375 is its
	 * ambient scale. This shader used the uniforms raw, so a card and the mesh
	 * it replaces were lit by two different lights in one framebuffer.
	 *
	 * AND THE SAME SPACE (lane IMPOSTORLIGHT1, 2026-09-22). `normal` is MODEL
	 * space (frameNormalModel above) and `lightSourcePosition` is VIEW space --
	 * the mesh path lights `n * normalMatrix` with `normalMatrix` the view
	 * rotation (glshape.cpp:658, fo4_default.vert:46,59). The old loop dotted
	 * the two raw, so under the headlight a card was lit by its model-space UP
	 * component: crown tops bright, the trunk faces that point at the camera --
	 * the mesh's brightest pixels -- dark. MEASURED on the five cardRes-512
	 * subjects at their bake directions: that normal agreed with the mesh's in
	 * sign 41..51 % on x and y (a coin toss), mean angle 69..88 deg; the same
	 * normal taken to view space agrees 80..99 %, 5..22 deg. The diffuse rung of
	 * the brightness ladder was where card/mesh fell from 0.97..1.01 (albedo,
	 * ambient) to 0.49..0.91.
	 *
	 * ONE-SIDED, like the mesh. The old `abs( dot )` lit the side facing AWAY
	 * from the light as brightly as the lit side. The mesh never does that, not
	 * even on a two-sided material: fo4_default.frag:404 flips a back face's
	 * normal to face the VIEWER and then clamps N.L at zero (:422), and the
	 * bake photographs exactly that flipped normal (channel 8, :305). So the
	 * baked normal already carries the two-sided flag's whole effect, and the
	 * clamp below is right for every material. Vanilla's own flag (the BGSM's
	 * bTwoSided, read by scratchpad/impostorlight1_20260922/material_flags.py):
	 * all three trees 1 (every shape, the trunk included -- one atlas material),
	 * the rock 0; none of the five has back-lighting or subsurface set, so no
	 * light-through term is owed either.
	 *
	 * The mesh path lights with light 0 only (fo4_default.vert:59,63), and so
	 * does this; its diffuse is Oren-Nayar at roughness 1 - smoothness times
	 * (1 - Fresnel).
	 *
	 * ROUGHNESS 1, not the sheet's gloss. The mesh's Oren-Nayar reads the
	 * material's smoothness and the card does carry it (mask.r), but the mask
	 * sheet is not shaded until the material renderer is ruled (the header of
	 * this file). Measured both ways, card/mesh mean luma, tonemapped, bake
	 * directions: roughness 1 -> 0.953 .. 0.984 on the five subjects; the
	 * sheet's gloss -> 0.979 .. 1.000; Lambert -> 0.981 .. 1.033. `nView` is
	 * declared above the debug channels so channel 13 shows exactly it. */
	const float smoothness = 0.0;
	vec3  V = ( projectionMatrix[3][3] == 1.0 ) ? vec3( 0.0, 0.0, 1.0 )
	        : normalize( -( modelViewMatrix * vec4( cardPosModel, 1.0 ) ).xyz );
	vec3  lit = vec3( 0.0 );
	vec3  L = lightSourcePosition[0].xyz;
	if ( dot( L, L ) > 1e-8 ) {
		L = normalize( L );
		float NdotL = dot( nView, L );
@@DIFF@@		lit = sqrt( max( lightSourceDiffuse[0].rgb, vec3( 0.0 ) ) ) * diff;
	}
'''.replace('@@DIFF@@', diffuse_expr)
s = s[:a] + NEW + s[b:]
DBG_ANCHOR = TAB + 'if ( debugChannel != 0 ) {' + NLc
assert s.count(DBG_ANCHOR) == 1
s = s.replace(DBG_ANCHOR, TAB + '/* The normal the LIGHT is dotted with, in the light' + chr(39) + 's (view) space --' + NLc
    + TAB + ' * lane IMPOSTORLIGHT1. Debug channel 13 shows exactly it, and the gate reads it. */' + NLc
    + TAB + 'vec3 nView = normalize( mat3( modelViewMatrix ) * normal );' + NLc + NLc + DBG_ANCHOR)
C12 = TAB*3 + 'dbg = vec3( max( t, 0.0 ), 0.0, max( -t, 0.0 ) );' + NLc + TAB*2 + '}' + NLc
assert s.count(C12) == 1
s = s.replace(C12, C12 + TAB*2 + 'else if ( debugChannel == 13 ) dbg = nView * 0.5 + 0.5;   // the LIT normal, view space' + NLc)

if mode == 'oren':
    FUNCS = '''
/* The mesh path's diffuse, verbatim fo4_default.frag:122-178 (OrenNayarFull),
 * and its Fresnel at the FO4 default power of 5 -- the card has no per-material
 * fresnelPower, and under the headlight V.H = 1 makes the power irrelevant. */
#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795
#endif
#define FLT_EPSILON 1.192092896e-07F

float fresnelSchlick5( float VdotH, float F0 )
{
	float e = pow( 1.0 - VdotH, 5.0 );
	return clamp( e + F0 * ( 1.0 - e ), 0.0, 1.0 );
}
'''
    fr = open(src.replace('impostor_oct.frag', 'fo4_default.frag'), 'rb').read().decode('utf-8')
    i = fr.find('float OrenNayarFull(float NdotL, float NdotV, float LdotV, float roughness)\n')
    j = fr.find('\treturn L1 + L2;\n}\n', i)
    assert i > 0 and j > i
    FUNCS += '\n' + fr[i:j + len('\treturn L1 + L2;\n}\n')]
    anchor = '/* CLAUSE C5 (spec 49) and SPEC GAP #6: unpack the frame\'s half-packed normal,\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor, FUNCS.lstrip('\n') + '\n' + anchor)

assert '\r' not in s
open(dst, 'wb').write(s.encode('utf-8'))
print('wrote %s (%s), %d bytes' % (dst, mode, len(s)))
