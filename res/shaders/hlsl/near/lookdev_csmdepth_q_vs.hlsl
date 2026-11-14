// HLSL source of features/near/lookdev_csmdepth_q.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "NearBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* The cascade caster pass through the render queues (lane GPUDRIVEN1): lookdev_csmdepth.vert with the shape's
 * model-view matrix read per draw from the queue's matrix buffer (locations 8-11, one per draw via baseInstance). */

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 csmClipFromView;
};


static float3 vertexPosition;
static float4x4 modelViewMatrix;

void vert_main()
{
	gl_Position = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), csmClipFromView);
}

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

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    modelViewMatrix[0] = stage_input.modelViewMatrix_0;
    modelViewMatrix[1] = stage_input.modelViewMatrix_1;
    modelViewMatrix[2] = stage_input.modelViewMatrix_2;
    modelViewMatrix[3] = stage_input.modelViewMatrix_3;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
