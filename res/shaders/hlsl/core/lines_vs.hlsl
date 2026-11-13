// HLSL source of lines.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

static float4 vsColor;

uniform row_major float4x4 modelViewMatrix;

uniform float4 vertexColorOverride;	// components greater than zero replace the vertex color
uniform float4 highlightColor;

static float3	vertexPosition;
static float4	vertexColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
};
struct SPIRV_Cross_Output
{
    WW_LOC(0) float4 vsColor : TEXCOORD0;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	float4	v = float4( vertexPosition, 1.0 );

	gl_Position = mul(v, modelViewMatrix);

	vsColor = select((vertexColorOverride > (float4)(0.0)), vertexColorOverride, vertexColor);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vsColor = vsColor;
    return stage_output;
}
