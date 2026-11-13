// HLSL source of skybox.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

static float3 LightDir;
static float3 ViewDir;

static float3x3 reflMatrix;

#include "uniforms.src.hlsli"

uniform bool invertZAxis;

static float3 vertexPosition;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float3 LightDir : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    nointerpolation float3x3 reflMatrix : TEXCOORD2;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	float4 v = float4( mul(vertexPosition, viewMatrix), 1.0 );

	ViewDir = float3(-v.xy, 1.0);
	LightDir = lightSourcePosition[0].xyz;

	reflMatrix = envMapRotation;
	if ( invertZAxis ) {
		reflMatrix[0][2] *= -1.0;
		reflMatrix[1][2] *= -1.0;
		reflMatrix[2][2] *= -1.0;
	}

	if ( projectionMatrix[3][3] == 1.0 )
		gl_Position = float4(0.0, 0.0, 2.0, 1.0);	// orthographic view is not supported
	else
		gl_Position = float4( ( mul(v, projectionMatrix) ).xy / clamp( projectionMatrix[0][0], 0.001, 1.0 ), 1.0, 1.0 );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.ViewDir = ViewDir;
    stage_output.LightDir = LightDir;
    stage_output.reflMatrix = reflMatrix;
    return stage_output;
}
