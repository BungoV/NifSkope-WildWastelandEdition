// HLSL source of sk_msn.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
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

static float4 A;
static float4 C;
static float4 D;

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
};


static float3 vertexPosition;
static float4 vertexColor;
static float2 multiTexCoord0;

#define BT_POSITION_ONLY 1
#include "bonetransform.hlsli"

void vert_main()
{
	float4	v = float4( vertexPosition, 1.0 );

	if ( boneWeights[0].x > 0.0 && doSkinning )
		boneTransform( v );

	v = mul(v, modelViewMatrix);
	gl_Position = mul(v, projectionMatrix);
	texCoord = multiTexCoord0;

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = float3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition[0].xyz;

	A = float4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale );
	C = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vertexColor);
	D = float4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float4 boneWeights[2] : TEXCOORD5;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float3 LightDir : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    nointerpolation float4 A : TEXCOORD3;
    float4 C : TEXCOORD4;
    nointerpolation float4 D : TEXCOORD5;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    multiTexCoord0 = stage_input.multiTexCoord0;
    boneWeights = stage_input.boneWeights;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.LightDir = LightDir;
    stage_output.ViewDir = ViewDir;
    stage_output.texCoord = texCoord;
    stage_output.A = A;
    stage_output.C = C;
    stage_output.D = D;
    return stage_output;
}
