// HLSL source of cell_shadowdepth.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "LightsBindingsVk.hlsli"
#include "../core/GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

/* Shadow caster vertex stage: model-view, then view -> light-relative world (shRelFromView), then the
 * cube face's projection (shClipFromRel: GL's face axes, 90 degrees). */

uniform row_major float4x4 shRelFromView;
uniform row_major float4x4 shClipFromRel;
uniform row_major float4x4 modelViewMatrix;

static float3 vertexPosition;
static float2 multiTexCoord0;	// lane GAPFIX1: alpha-tested casters' first UV set

static float3 shRel;
static float2 texCoord;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float2 multiTexCoord0 : TEXCOORD7;
};
struct SPIRV_Cross_Output
{
    float3 shRel : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	float4 rel = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), shRelFromView);
	shRel = rel.xyz;
	texCoord = multiTexCoord0;
	gl_Position = mul(float4( rel.xyz, 1.0 ), shClipFromRel);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.shRel = shRel;
    stage_output.texCoord = texCoord;
    return stage_output;
}
