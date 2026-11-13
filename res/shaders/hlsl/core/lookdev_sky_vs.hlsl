// HLSL source of lookdev_sky.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* Lookdev background (lane PBRR2B), vertex stage: one full-screen quad in NDC.
 * The fragment stage turns each pixel into its view ray through the SAME
 * projection the model is drawn with, so the background and the model's
 * reflections agree on the field of view. */

#include "uniforms.hlsli"

static float3 vertexPosition;	// -1..1 NDC

static float2 ndc;

void vert_main()
{
	ndc = vertexPosition.xy;
	gl_Position = float4( vertexPosition.xy, 1.0, 1.0 );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float2 ndc : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.ndc = ndc;
    return stage_output;
}
