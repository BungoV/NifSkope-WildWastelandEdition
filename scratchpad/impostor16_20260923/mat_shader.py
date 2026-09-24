"""IMPOSTOR16 -- the card's FULL MATERIAL, look-test only, behind WW_IMPOSTOR_CHANNEL=20.

    python mat_shader.py <in impostor_oct.frag> <out impostor_oct.frag>

Channel 20 is not a debug view: it runs the lit path with the gloss and specular
the bake stored in _gsaos (R gloss, G specular), the mesh path's GGX + Smith +
Schlick lobe (fo4_default.frag:446-476) and its ambient-specular term (:534).
Channel 0 (the shipped default) is byte-for-byte the same code path as before:
smoothness 0, no specular. The file is written to a RUN FOLDER, never to
res/shaders or release/shaders. LF only, exact-once anchors.
"""
import sys

src, dst = sys.argv[1], sys.argv[2]
b = open(src, 'rb').read()
assert b.count(b'\r') == 0


def once(old, new):
    global b
    n = b.count(old)
    assert n == 1, (old[:60], n)
    b = b.replace(old, new)


once(b'float OrenNayarFull(float NdotL, float NdotV, float LdotV, float roughness)\n',
     b'''/* IMPOSTOR16 look test: the mesh path's GGX + Smith, verbatim fo4_default.frag. */
float D_GGX( float NdotH, float alpha )
{
	float a2 = alpha * alpha;
	float d = NdotH * NdotH * ( a2 - 1.0 ) + 1.0;
	return a2 / max( M_PI * d * d, 0.0001 );
}
float G1( float NdotX, float k )
{
	return NdotX / max( NdotX * ( 1.0 - k ) + k, 0.0001 );
}

float OrenNayarFull(float NdotL, float NdotV, float LdotV, float roughness)
''')
once(b'\tif ( debugChannel != 0 ) {\n',
     b'\tbool fullMat =( debugChannel == 20 );   // IMPOSTOR16 look test\n'
     b'\tif ( debugChannel != 0 && !fullMat ) {\n')
once(b'\tconst float smoothness = 0.0;\n',
     b'\tconst float smoothness0 = 0.0;\n'
     b'\tfloat smoothness = ( fullMat && hasMaskSheet ) ? clamp( mask.r, 0.0, 1.0 ) : smoothness0;\n'
     b'\tfloat specMask   = ( fullMat && hasMaskSheet ) ? clamp( mask.g, 0.0, 1.0 ) : 0.0;\n'
     b'\tvec3  specLit    = vec3( 0.0 );\n')
once(b'\t\tlit = sqrt( max( lightSourceDiffuse[0].rgb, vec3( 0.0 ) ) ) * diff;\n',
     b'''\t\tlit = sqrt( max( lightSourceDiffuse[0].rgb, vec3( 0.0 ) ) ) * diff;
		if ( fullMat ) {
			vec3  Dl = sqrt( max( lightSourceDiffuse[0].rgb, vec3( 0.0 ) ) );
			vec3  Al = sqrt( max( lightSourceAmbient.rgb, vec3( 0.0 ) ) ) * 0.375;
			vec3  H  = normalize( L + V );
			float NdotL0 = max( NdotL, FLT_EPSILON );
			float NdotV  = max( dot( nView, V ), FLT_EPSILON );
			float NdotH  = max( dot( nView, H ), FLT_EPSILON );
			float VdotH  = max( dot( V, H ), FLT_EPSILON );
			float rough  = clamp( 1.0 - smoothness, 0.02, 1.0 );
			float kS     = ( rough + 1.0 ) * ( rough + 1.0 ) * 0.125;
			float F      = fresnelSchlick5( VdotH, 0.04 );
			specLit = vec3( D_GGX( NdotH, rough * rough ) * G1( NdotL0, kS ) * G1( NdotV, kS )
			                * F / max( 4.0 * NdotL0 * NdotV, 0.001 ) ) * specMask * NdotL0 * Dl;
			specLit += Al * specMask * F * ( 1.0 - NdotV ) * Dl;
			/* The light-through terms the MESH draws for this material
			 * (fo4_default.frag:492-516, 530, 536), with the vanilla BGSM's own
			 * numbers because the .lodm does not carry them:
			 * MapleInstituteAtlasGreen.BGSM backlightPower 4.0 (the shader
			 * clamps it to 1), subsurface on, rolloff 0.7. Gated by the mask
			 * sheet's A (the bake's alpha-tested-shape flag = the subsurface
			 * mask), which is 1 on every shape of this one-material tree. */
			float sss = hasMaskSheet ? mask.a : 1.0;
			float diffRaw = OrenNayarFull( NdotL, dot( nView, V ), dot( L, V ), 1.0 - smoothness );
			float wrap = ( NdotL + 0.7 ) / 1.7;
			vec3  soft = colour.rgb * max( 0.0, wrap ) * smoothstep( 1.0, 0.0, sqrt( diffRaw ) );
			specLit += sss * soft * colour.rgb * Dl * ( 1.0 - F );
			float NdotNegL = max( dot( nView, -L ), FLT_EPSILON );
			specLit += sss * colour.rgb * NdotNegL * 1.0 * Dl * glowScaleSRGB;
		}
''')
once(b'\tvec3 lc = colour.rgb * ( lit + ambient );\n',
     b'\tvec3 lc = colour.rgb * ( lit + ambient ) + specLit;\n')
open(dst, 'wb').write(b)
print('written', dst, 'CR', b.count(b'\r'))
