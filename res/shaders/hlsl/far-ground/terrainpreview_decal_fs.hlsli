// HLSL source of features/far-ground/terrainpreview_decal_fs.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float uMaxLod;
	int uDecalNormals;
#else
Texture2D<float4> uAtlasC : register(t0, space0); SamplerState _uAtlasC_sampler : register(s0, space0);
Texture2D<float4> uAtlasN : register(t1, space0); SamplerState _uAtlasN_sampler : register(s1, space0);
static float4 oAlbedo;
static float4 oNormal;
void main()
{
	float3 w, l;
	Rec r;
	if ( !unproject( w, l, r ) )
		discard;
	float2 uv = float2( ( l.x - r.box0.x ) / ( r.box0.z - r.box0.x ), ( r.box0.w - l.y ) / ( r.box0.w - r.box0.y ) );
	float2 a = lerp( r.atlas.xy, r.atlas.zw, uv );
	float pix = uOrtho != 0 ? uPixelK : uPixelK * distance( w, uEye );
	float lod = clamp( log2( max( 1.0, pix / ( r.box1.z * r.posScale.w ) ) ), 0.0, uMaxLod );
	float4 c = uAtlasC.SampleLevel( _uAtlasC_sampler, a, lod );
	float4 n = uAtlasN.SampleLevel( _uAtlasN_sampler, a, lod );
	float wgt = 1.0;
	if ( uOption == 1 )
		wgt = 1.0 - smoothstep( uFade.x, uFade.y, distance( w, uEye ) );
	oAlbedo = float4( c.rgb, c.a * wgt );
	float3 nw = normalize( qrot( r.quat, n.rgb * 2.0 - 1.0 ) );
	oNormal = float4( nw * 0.5 + 0.5, uDecalNormals != 0 ? n.a * c.a * wgt : 0.0 );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/far-ground/terrainpreview_decal_fs.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
