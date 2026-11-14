// HLSL source of features/gi/proberelight_blend.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
void main()
{
	int s = int( gl_GlobalInvocationID.x );
	if ( s >= uN )
		return;
	int b = feedStart[s], e = feedStart[s + 1];
	if ( b == e )
		return;
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = 0.0;
	float wsum = 0.0;
	for ( int k = b; k < e; k++ ) {
		int rec = asint( feed[k * 4 + 2] );
		float vf = rec >= 0 ? doorF( rec, 0, true ) : 1.0;
		if ( vf <= 0.0 )
			continue;
		int p = asint( feed[k * 4] );
		float w = feed[k * 4 + 1] * vf;
		wsum += w;
		for ( int q = 0; q < 18; q++ )
			acc[q] += w * pe[p * 18 + q];
	}
	int v = slots[s * 2], which = slots[s * 2 + 1];
	int base = which * uNVox * 24;
	for ( int a = 0; a < 6; a++ ) {
		int o = base + ( a * uNVox + v ) * 4;
		for ( int c = 0; c < 3; c++ )
			grid[o + c] = wsum > 0.0 ? acc[a * 3 + c] / wsum : 0.0;
		grid[o + 3] = 1.0;
	}
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/gi/proberelight_blend.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
