// HLSL source of lookdev_dome.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

/* Lookdev weather sky dome (lane PBRWX1, spec_weather_sky.md s2.1):
 * linear = SkyScale * (r*Horizon + g*SkyLower + b*SkyUpper), each colour the
 * CIELab-blended NAM0 row to the power 2.2 (done on the CPU). Out through the
 * Studio output path. Not fogged (ruling). */

#include "uniforms.src.hlsli"

uniform float3 skyH;
uniform float3 skyL;
uniform float3 skyU;
uniform float skyScale;
uniform float alphaMul;
uniform float sceneExposure;
uniform int viewTransform;

static float4 vcol;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float4 vcol : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


#include "lookdev_output.src.hlsli"

void frag_main()
{
	float3 c = skyScale * ( vcol.r * skyH + vcol.g * skyL + vcol.b * skyU );
	fragColor = float4( studioOutput( c ), alphaMul );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vcol = stage_input.vcol;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
