// HLSL source of features/sky/atmo_skyview.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane ATMO1: the sky-view LUT (Hillaire 2020 sec. 5.3), one light (the sun, or the moon on its own LUT):
// single scattering through the transmittance LUT + the multiple-scattering term (atmo_common.glsl atSkyMarch)

#define WW_LOOSE_TYPES 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_UNIFORMS
	float2 atMsSize;
	float2 atViewSize;
	float atCamR;	// km from the planet center
	float atLightMu;	// cos zenith of the light
	float atMsOn;	// 0 under the red "noms"
	float atGround;	// lane SKY2B: 1 = the lit ground below the horizon; 0 under the red "noground"
};
#include "atmo_common.hlsli"

Texture2D<float4> atTransTex : register(t0, space0); SamplerState _atTransTex_sampler : register(s0, space0);
Texture2D<float4> atMsTex : register(t1, space0); SamplerState _atMsTex_sampler : register(s1, space0);

static float4 fragColor;

void frag_main()
{
	float cosZ, cosAz;
	atSkyUvToDir( gl_FragCoord.xy / atViewSize, atViewSize, atCamR, cosZ, cosAz );
	float sinZ = sqrt( max( 0.0, 1.0 - cosZ * cosZ ) );
	float3 v = float3( sinZ * cosAz, sinZ * sqrt( max( 0.0, 1.0 - cosAz * cosAz ) ), cosZ );
	float3 s = float3( sqrt( max( 0.0, 1.0 - atLightMu * atLightMu ) ), 0.0, atLightMu );
	float3 L = atSkyMarch( atTransTex, _atTransTex_sampler, atMsTex, _atMsTex_sampler, atMsSize, atMsOn, atGround, atCamR, v, s );
	fragColor = float4( L, 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
