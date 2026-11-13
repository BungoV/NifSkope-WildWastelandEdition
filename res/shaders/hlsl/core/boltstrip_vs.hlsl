// HLSL source of boltstrip.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

// Textured camera-facing strip for the BSProceduralLightningController preview

static float4 C;
static float2 texCoord;

#include "uniforms.src.hlsli"

uniform row_major float4x4 modelViewMatrix;

static float3	vertexPosition;
static float4	vertexColor;
static float2	vertexUV;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float2 vertexUV : TEXCOORD2;
};
struct SPIRV_Cross_Output
{
    float4 C : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	C = vertexColor;
	texCoord = vertexUV;

	gl_Position = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), projectionMatrix);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexColor = stage_input.vertexColor;
    vertexUV = stage_input.vertexUV;
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.C = C;
    stage_output.texCoord = texCoord;
    return stage_output;
}
