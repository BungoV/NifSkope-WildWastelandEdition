// HLSL source of lookdev_dome.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
/* Lookdev weather sky dome (lane PBRWX1, spec_weather_sky.md s2.1):
 * linear = SkyScale * (r*Horizon + g*SkyLower + b*SkyUpper), each colour the
 * CIELab-blended NAM0 row to the power 2.2 (done on the CPU). Out through the
 * Studio output path. Not fogged (ruling). */

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	float3 skyH;
	float3 skyL;
	float3 skyU;
	float skyScale;
	float alphaMul;
	float sceneExposure;
	int viewTransform;
};


static float4 vcol;

static float4 fragColor;

#include "lookdev_output.hlsli"

void frag_main()
{
	float3 c = skyScale * ( vcol.r * skyH + vcol.g * skyL + vcol.b * skyU );
	fragColor = float4( studioOutput( c ), alphaMul );
}

struct SPIRV_Cross_Input
{
    float4 vcol : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vcol = stage_input.vcol;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
