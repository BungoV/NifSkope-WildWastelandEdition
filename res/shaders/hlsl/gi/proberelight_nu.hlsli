// HLSL source of features/gi/proberelight_nu.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
void main()
{
	int j = int( gl_GlobalInvocationID.x );
	if ( j >= uN )
		return;
	float sum = 0.0, sq = 0.0;
	for ( int k = linkStart[j]; k < linkStart[j + 1]; k++ ) {
		int o = k * 12;
		int s = asint( links[o] );
		int rec = asint( links[o + 11] );
		float3 w = float3( bsrc[s * 4], bsrc[s * 4 + 1], bsrc[s * 4 + 2] ) * float3( links[o + 2], links[o + 3], links[o + 4] ) * links[o + 1];
		if ( rec >= 0 )
			w *= float3( doorF( rec, 0, false ), doorF( rec, 1, false ), doorF( rec, 2, false ) );
		float cs = 0.0;
		for ( int a = 0; a < 6; a++ )
			cs += links[o + 5 + a];
		float x = dot( w, float3( 0.2126, 0.7152, 0.0722 ) ) * cs / 6.0;
		sum += x;
		sq += x * x;
	}
	float sky = 0.0;
	if ( uSkyOn != 0 ) {
		for ( int a = 0; a < 6; a++ )
			sky += dot( float3( probeK[j * 20 + 2 + a * 3], probeK[j * 20 + 3 + a * 3], probeK[j * 20 + 4 + a * 3] ),
				float3( 0.2126, 0.7152, 0.0722 ) );
		sky /= 6.0;
	}
	float kU = probeK[j * 20];
	float m1 = kU * sum + sky;
	bdst[j] = m1 > 0.0 ? min( kU * sqrt( sq ) / m1, 1.0 ) : 0.0;
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/gi/proberelight_nu.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
