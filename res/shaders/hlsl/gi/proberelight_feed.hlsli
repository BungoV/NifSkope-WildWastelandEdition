// HLSL source of features/gi/proberelight_feed.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
void main()
{
	int i = int( gl_GlobalInvocationID.x );
	if ( i >= uN )
		return;
	int so = i * 16;
	float3 n = float3( surf[so], surf[so + 1], surf[so + 2] );
	float3 E = (float3)( 0.0 );
	float ws = 0.0;
	for ( int k = feedStart[i]; k < feedStart[i + 1]; k++ ) {
		int rec = asint( feed[k * 4 + 2] );
		float vf = rec >= 0 ? doorF( rec, 0, true ) : 1.0;
		if ( vf <= 0.0 )
			continue;
		int p = asint( feed[k * 4] );
		float w = feed[k * 4 + 1] * vf;
		ws += w;
		for ( int a = 0; a < 3; a++ ) {
			int ax = 2 * a + ( n[a] >= 0.0 ? 0 : 1 );
			int po = p * 18 + ax * 3;
			E += w * n[a] * n[a] * float3( pe[po], pe[po + 1], pe[po + 2] );
		}
	}
	float3 alb = uRedGrow != 0 ? (float3)( 1.5 ) : float3( surf[so + 4], surf[so + 5], surf[so + 6] );
	float3 B1 = float3( surf[so + 12], surf[so + 13], surf[so + 14] );
	float3 Bn = B1 + ( ws > 0.0 ? alb * ( E / ws ) / 3.14159265358979 : (float3)( 0.0 ) );
	float3 Bo = float3( bsrc[i * 4], bsrc[i * 4 + 1], bsrc[i * 4 + 2] );
	bdst[i * 4] = Bn.x;
	bdst[i * 4 + 1] = Bn.y;
	bdst[i * 4 + 2] = Bn.z;
	float3 ch = abs( Bn - Bo );
	InterlockedMax( ctl[0], asuint( max( max( ch.x, ch.y ), ch.z ) ) );
	InterlockedMax( ctl[1], asuint( max( max( max( Bn.x, Bn.y ), Bn.z ), 0.0 ) ) );
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/gi/proberelight_feed.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
