// HLSL source of features/gi/proberelight_direct.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
void main()
{
	int i = int( gl_GlobalInvocationID.x );
	if ( i >= uN )
		return;
	float3 E = (float3)( 0.0 );
	for ( int k = pairStart[i]; k < pairStart[i + 1]; k++ ) {
		int li = asint( pairs[k * 8] );
		float d = pairs[k * 8 + 1], nl = pairs[k * 8 + 2], dl = pairs[k * 8 + 3];
		int rec = asint( pairs[k * 8 + 4] );
		int lo = li * 16;
		float3 col = float3( lights[lo + 4], lights[lo + 5], lights[lo + 6] );
		float r = lights[lo + 3];
		if ( or( all(col == (float3)( 0.0 )), d >= r ) )
			continue;
		float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
		float a;
		float kind = lights[lo + 15];   // lane LIGHTPHYS2: wwlp::lightTerm, decided per light on the CPU
		if ( kind < 0.5 ) {
			float xe = lights[lo + 9] > 0.0 ? pow( x, lights[lo + 9] ) : 1.0;
			float kk = 1.0 - clamp( lights[lo + 8] * xe + lights[lo + 7], 0.0, 1.0 );
			a = kk > 0.0 ? pow( kk, 2.2 ) : 0.0;
		} else if ( kind < 1.5 ) {   // wwlp::physRaw
			float s = lights[lo + 14], x2 = x * x;
			float w = clamp( 1.0 - x2 * x2, 0.0, 1.0 ), den = x2 + s * s;
			a = den > 0.0 ? lights[lo + 13] * ( w * w / den ) : 0.0;
		} else {   // wwlp::advAtt
			float t = clamp( ( x - 0.8 ) / ( 1.0 - 0.8 ), 0.0, 1.0 );
			float w = kind > 2.5 ? 1.0 : 1.0 - t * t * ( 3.0 - 2.0 * t );
			float m = max( lights[lo + 14], x );
			a = lights[lo + 13] * w / ( m * m );
		}
		if ( lights[lo + 12] > 0.5 ) {
			float base = clamp( 1.0 - ( 1.0 - dl ) / max( 1.0 - lights[lo + 11], 1e-4 ), 0.0, 1.0 );
			a *= base > 0.0 ? min( pow( base, max( lights[lo + 10], 1e-3 ) ), 1.0 ) : 0.0;
		}
		if ( a * nl <= 0.0 )
			continue;
		float3 f = float3( doorF( rec, 0, false ), doorF( rec, 1, false ), doorF( rec, 2, false ) );
		E += col * ( a * nl ) * f;
	}
	int so = i * 16;
	if ( uHasDir != 0 && surf[so + 3] > 0.0 )
		E += uDirColor * surf[so + 3];
	if ( uSunOn != 0 && surf[so + 7] > 0.0 )
		E += uSun * surf[so + 7];
	float3 B = float3( surf[so + 4], surf[so + 5], surf[so + 6] ) * E + float3( surf[so + 8], surf[so + 9], surf[so + 10] );
	surf[so + 12] = B.x;
	surf[so + 13] = B.y;
	surf[so + 14] = B.z;
	bsrc[i * 4] = B.x;
	bsrc[i * 4 + 1] = B.y;
	bsrc[i * 4 + 2] = B.z;
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/gi/proberelight_direct.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
