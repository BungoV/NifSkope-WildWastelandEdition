// HLSL source of features/far-ground/terrainpreview_light_fs.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	row_major float4x4 uInvViewProj;
	float4 uAoMap;
	float uAoLod;
	int uAoMode;
#else
static float2 vUv;
static float4 oColour;
Texture2D<float4> uAlbedo : register(t0, space0); SamplerState _uAlbedo_sampler : register(s0, space0);
Texture2D<float4> uNormal : register(t1, space0); SamplerState _uNormal_sampler : register(s1, space0);
Texture2D<float4> uDepth : register(t2, space0); SamplerState _uDepth_sampler : register(s2, space0);
Texture2D<float4> uAo : register(t3, space0); SamplerState _uAo_sampler : register(s3, space0);
void main()
{
	int2 p = int2( gl_FragCoord.xy );
	float d = uDepth.Load( int3(p, 0) ).r;
	if ( d >= 1.0 ) {
		oColour = float4( 0.55, 0.64, 0.78, 1.0 );
		return;
	}
	float3 a = uAlbedo.Load( int3(p, 0) ).rgb;
	float3 n = normalize( uNormal.Load( int3(p, 0) ).rgb * 2.0 - 1.0 );
	float3 L = normalize( float3( 0.35, 0.45, 0.82 ) );
	float ao = 1.0;
	if ( uAoMode != 0 ) {
		float4 ndc = float4( vUv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0 );
		float4 wp = mul(ndc, uInvViewProj);
		float3 w = wp.xyz / wp.w;
		float2 uv = float2( ( w.x - uAoMap.x ) * uAoMap.z, ( uAoMap.y - w.y ) * uAoMap.w );
		ao = uAo.SampleLevel( _uAo_sampler, uv, uAoLod ).r;
	}
	oColour = float4( a * ( 0.45 + 0.75 * max( 0.0, dot( n, L ) ) ) * ao, 1.0 );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/far-ground/terrainpreview_light_fs.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
