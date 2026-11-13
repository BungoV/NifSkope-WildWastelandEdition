// HLSL source of features/far-ground/terrainpreview_decal_fs_head.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	row_major float4x4 uInvViewProj;
	float2 uViewport;
	float3 uEye;
	float2 uFade;
	int uOption;
	int uOrtho;
	float uPixelK;
#else
static int vRec;
Texture2D<float4> uDepth : register(t0, space0); SamplerState _uDepth_sampler : register(s0, space0);
bool unproject( out float3 w, out float3 l, out Rec r )
{
	r = recs[vRec];
	float d = uDepth.Load( int3(int2( gl_FragCoord.xy ), 0) ).r;
	if ( d >= 1.0 )
		return false;
	float4 ndc = float4( gl_FragCoord.xy / uViewport * 2.0 - 1.0, d * 2.0 - 1.0, 1.0 );
	float4 wp = mul(ndc, uInvViewProj);
	w = wp.xyz / wp.w;
	float4 qi = float4( -r.quat.xyz, r.quat.w );
	l = qrot( qi, w - r.posScale.xyz ) / r.posScale.w;
	return l.x >= r.box0.x && l.x <= r.box0.z && l.y >= r.box0.y && l.y <= r.box0.w
		&& l.z >= r.box1.x && l.z <= r.box1.y;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/far-ground/terrainpreview_decal_fs_head.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): unproject
#endif	// WW_HLSL_SRC
