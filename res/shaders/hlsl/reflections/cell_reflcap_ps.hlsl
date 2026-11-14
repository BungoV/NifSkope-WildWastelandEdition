// HLSL source of features/reflections/cell_reflcap.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ReflectionsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
/* lane REFLPROBE1: the G-buffer texel. Target 0: the albedo as the colour pass takes it (base map x vertex colour,
 * before its square), a = 1 (a hit; the clear leaves 0 = the sky). Target 1: the world normal turned to face the
 * probe, and the distance to it. The alpha test is the colour pass's (base alpha x material alpha vs threshold). */

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	bool rpHasBase;
	bool rpHasVc;
	bool rpAlphaTest;
	float alpha;
	float alphaThreshold;
};


static float3 rpRel;
static float3 rpN;
static float4 rpC;
static float2 rpUv;

static float4 rpAlbedo;
static float4 rpNormalDist;

void frag_main()
{
	float4 b = rpHasBase ? BaseMap.Sample( _BaseMap_sampler, rpUv ) : float4( 0.5, 0.5, 0.5, 1.0 );
	if ( rpAlphaTest && b.a * alpha < alphaThreshold )
		discard;
	float3 c = rpHasVc ? rpC.rgb : (float3)( 1.0 );
	float3 n = normalize( rpN );
	if ( dot( n, rpRel ) > 0.0 )
		n = -n;	// a two-sided or flipped face: the side the probe sees
	rpAlbedo = float4( clamp( b.rgb * c, 0.0, 1.0 ), 1.0 );
	rpNormalDist = float4( n, length( rpRel ) );
}

struct SPIRV_Cross_Input
{
    float3 rpRel : TEXCOORD0;
    float3 rpN : TEXCOORD1;
    float4 rpC : TEXCOORD2;
    float2 rpUv : TEXCOORD3;
};

struct SPIRV_Cross_Output
{
    float4 rpAlbedo : SV_Target0;
    float4 rpNormalDist : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    rpRel = stage_input.rpRel;
    rpN = stage_input.rpN;
    rpC = stage_input.rpC;
    rpUv = stage_input.rpUv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.rpAlbedo = rpAlbedo;
    stage_output.rpNormalDist = rpNormalDist;
    return stage_output;
}
