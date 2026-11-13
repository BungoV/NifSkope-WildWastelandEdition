// HLSL source of uvedit.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

static float2 texCoord;

uniform float4 viewScaleAndOffset;

static float2	multiTexCoord0;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float2 multiTexCoord0 : TEXCOORD7;
};
struct SPIRV_Cross_Output
{
    float2 texCoord : TEXCOORD0;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	float2	offs = multiTexCoord0;

	gl_Position = float4( offs * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 ), (float2)(1.0) );

	texCoord = offs * viewScaleAndOffset.xy + viewScaleAndOffset.zw;
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.texCoord = texCoord;
    return stage_output;
}
