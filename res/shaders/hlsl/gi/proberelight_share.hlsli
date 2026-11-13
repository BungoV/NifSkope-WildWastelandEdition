// HLSL source of features/gi/proberelight_share.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
void main()
{
	int j = int( gl_GlobalInvocationID.x );
	if ( j >= uN )
		return;
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = pe[j * 18 + q];
	float as = 0.0, nu = bdst[j];
	for ( int k = feedStart[j]; k < feedStart[j + 1]; k++ ) {
		int rec = asint( feed[k * 4 + 2] );
		float vf = rec >= 0 ? doorF( rec, 0, true ) : 1.0;
		float a = nu * feed[k * 4 + 1] * vf;
		if ( a <= 0.0 )
			continue;
		as += a;
		int p = asint( feed[k * 4] );
		for ( int q = 0; q < 18; q++ )
			acc[q] += a * pe[p * 18 + q];
	}
	for ( int q = 0; q < 18; q++ )
		bsrc[j * 18 + q] = acc[q] / ( 1.0 + as );
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/gi/proberelight_share.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
