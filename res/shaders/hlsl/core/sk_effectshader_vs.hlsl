// HLSL source of sk_effectshader.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
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

static float2 texCoord;

static float3 N;

static float4 C;

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float3x3 normalMatrix;			// in row-major order
	row_major float4x4 modelViewMatrix;
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
};


static float3 vertexPosition;
static float4 vertexColor;
static float3 normalVector;
static float2 multiTexCoord0;

#define BT_NO_TANGENTS 1
#include "bonetransform.hlsli"

void vert_main()
{
	float4	v = float4( vertexPosition, 1.0 );
	float3	n = normalVector;

	if ( boneWeights[0].x > 0.0 && doSkinning )
		boneTransform( v, n );

	v = mul(v, modelViewMatrix);
	gl_Position = mul(v, projectionMatrix);
	texCoord = multiTexCoord0;

	N = normalize( mul(normalMatrix, n) );

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = float3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition[0].xyz;

	C = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vertexColor);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float3 normalVector : TEXCOORD2;
    float4 boneWeights[2] : TEXCOORD5;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(5) float3 N : TEXCOORD5;
    WW_LOC(1) float4 C : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    normalVector = stage_input.normalVector;
    multiTexCoord0 = stage_input.multiTexCoord0;
    boneWeights = stage_input.boneWeights;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.LightDir = LightDir;
    stage_output.ViewDir = ViewDir;
    stage_output.texCoord = texCoord;
    stage_output.N = N;
    stage_output.C = C;
    return stage_output;
}
