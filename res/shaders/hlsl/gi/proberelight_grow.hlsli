// HLSL source of features/gi/proberelight_grow.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
void main()
{
	int g = uFirst + int( gl_GlobalInvocationID.x );
	if ( int( gl_GlobalInvocationID.x ) >= uN )
		return;
	int s = linkStart[g];
	int b = pairStart[g], e = pairStart[g + 1];
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = 0.0;
	for ( int k = b; k < e; k++ ) {
		int q = feedStart[k];
		int base = slots[q * 2 + 1] * uNVox * 24;
		for ( int a = 0; a < 6; a++ ) {
			int o = base + ( a * uNVox + slots[q * 2] ) * 4;
			for ( int c = 0; c < 3; c++ )
				acc[a * 3 + c] += grid[o + c];
		}
	}
	int base = slots[s * 2 + 1] * uNVox * 24;
	float n = float( e - b );
	for ( int a = 0; a < 6; a++ ) {
		int o = base + ( a * uNVox + slots[s * 2] ) * 4;
		for ( int c = 0; c < 3; c++ )
			grid[o + c] = acc[a * 3 + c] / n;
		grid[o + 3] = 1.0;
	}
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/gi/proberelight_grow.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
