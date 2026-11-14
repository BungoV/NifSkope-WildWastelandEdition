// HLSL source of skybox.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static float3 LightDir;
static float3 ViewDir;

static float3x3 reflMatrix;

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	bool invertZAxis;
};


static float3 vertexPosition;

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

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.LightDir = LightDir;
    stage_output.ViewDir = ViewDir;
    stage_output.reflMatrix = reflMatrix;
    return stage_output;
}
