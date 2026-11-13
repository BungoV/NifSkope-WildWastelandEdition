// HLSL source of lookdev_sky.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

/* Lookdev background (lane PBRR2B), vertex stage: one full-screen quad in NDC.
 * The fragment stage turns each pixel into its view ray through the SAME
 * projection the model is drawn with, so the background and the model's
 * reflections agree on the field of view. */

#include "uniforms.src.hlsli"

static float3 vertexPosition;	// -1..1 NDC

static float2 ndc;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float2 ndc : TEXCOORD0;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	ndc = vertexPosition.xy;
	gl_Position = float4( vertexPosition.xy, 1.0, 1.0 );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.ndc = ndc;
    return stage_output;
}
