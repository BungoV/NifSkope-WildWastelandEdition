// HLSL source of features/far-ground/terrainpreview_terrain_vs.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	row_major float4x4 uViewProj;
	float2 uOrigin;
	float uSpacing;
#else
static float2 aIJ;
Texture2D<float4> uHeight : register(t0, space0); SamplerState _uHeight_sampler : register(s0, space0);
static float3 vWorld;
void main()
{
	float z = uHeight.Load( int3(int2( aIJ ), 0) ).r;
	vWorld = float3( uOrigin + aIJ * uSpacing, z );
	gl_Position = mul(float4( vWorld, 1.0 ), uViewProj);
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/far-ground/terrainpreview_terrain_vs.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): main
#endif	// WW_HLSL_SRC
