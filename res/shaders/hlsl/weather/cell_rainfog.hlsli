// HLSL source of features/weather/cell_rainfog.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	int rfOn;			// 1: the rain fog this frame
	row_major float4x4 rfFromView;	// view space -> (u, v, the height below the Low band's base in km)
	float4 rfLaw;			// x sigma per game unit near the ground (the rain's share in), y the base excess,
	// z 1 / the falloff (km), w 1 / the fade above the base (km)
	float2 rfFar;			// x the band's coverage past the map, y the edge band (uv) where the map fades to it
#else
/* lane CLOUD6e (RDR2 gap e, Bauer 2019 slide 19): the rain fog under the rain clouds, read by the air's froxel pass
 * (cell_volair.frag: one read a sub-step, added to its optical depth). The law and its numbers: src/features/weather/rainfogmath.h;
 * the uniforms: src/gl/cellvolair.cpp. rfOn 0 (a weather that does not rain, the red, no cloud map, indoors): never
 * read, the air exactly as before. */


Texture2D<float4> rfMap : register(t19, space0); SamplerState _rfMap_sampler : register(s19, space0);	// CLOUD1's cloud map (R: the Low band's coverage), linear, clamped

// the rain fog's extinction per game unit at view point posView
float rfSigma( float3 posView )
{
	float3 q = ( mul(float4( posView, 1.0 ), rfFromView) ).xyz;
	float2 e = abs( q.xy - 0.5 ) * 2.0;
	float c = lerp( rfMap.SampleLevel( _rfMap_sampler, q.xy, 0.0 ).r, rfFar.x, smoothstep( 1.0 - rfFar.y, 1.0, max( e.x, e.y ) ) );
	float p = q.z >= 0.0 ? 1.0 + rfLaw.y * exp( -q.z * rfLaw.z ) : ( 1.0 + rfLaw.y ) * max( 1.0 + q.z * rfLaw.w, 0.0 );
	return rfLaw.x * c * p;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/weather/cell_rainfog.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): rfSigma
#endif	// WW_HLSL_SRC
