// HLSL source of lookdev_csmdepth_q.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "NearBindingsVk.hlsli"
#include "../core/GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

/* The cascade caster pass through the render queues (lane GPUDRIVEN1): lookdev_csmdepth.vert with the shape's
 * model-view matrix read per draw from the queue's matrix buffer (locations 8-11, one per draw via baseInstance). */

uniform row_major float4x4 csmClipFromView;

static float3 vertexPosition;
static float4x4 modelViewMatrix;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 modelViewMatrix_0 : TEXCOORD8_0;
    float4 modelViewMatrix_1 : TEXCOORD8_1;
    float4 modelViewMatrix_2 : TEXCOORD8_2;
    float4 modelViewMatrix_3 : TEXCOORD8_3;
};
struct SPIRV_Cross_Output
{
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	gl_Position = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), csmClipFromView);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    modelViewMatrix[0] = stage_input.modelViewMatrix_0;
    modelViewMatrix[1] = stage_input.modelViewMatrix_1;
    modelViewMatrix[2] = stage_input.modelViewMatrix_2;
    modelViewMatrix[3] = stage_input.modelViewMatrix_3;
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
