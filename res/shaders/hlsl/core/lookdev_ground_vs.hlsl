// HLSL source of lookdev_ground.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

/* Lookdev ground plane (lane PBRR2B), vertex stage. The quad arrives in the
 * scene's world space (Z up); texture coordinates come from world XY. */

#include "uniforms.src.hlsli"

uniform row_major float4x4 modelViewMatrix;

static float3 vertexPosition;

static float3 worldPos;
static float3 viewPos;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float3 worldPos : TEXCOORD0;
    float3 viewPos : TEXCOORD1;
    float4 gl_Position : SV_Position;
};
	// lane FOG1: the fog reads the view-space position

void vert_main()
{
	worldPos = vertexPosition;
	float4 v = mul(float4( vertexPosition, 1.0 ), modelViewMatrix);
	viewPos = v.xyz;
	gl_Position = mul(v, projectionMatrix);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.worldPos = worldPos;
    stage_output.viewPos = viewPos;
    return stage_output;
}
