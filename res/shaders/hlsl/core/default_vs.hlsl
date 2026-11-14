// HLSL source of default.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
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

static float2 texCoords[9];

static float4 A;
static float4 C;
static float4 D;

static float3 N;

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
static float2 multiTexCoord1;
static float2 multiTexCoord2;
static float2 multiTexCoord3;
static float2 multiTexCoord4;
static float2 multiTexCoord5;
static float2 multiTexCoord6;
static float2 multiTexCoord7;
static float2 multiTexCoord8;

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
	texCoords[0] = multiTexCoord0;
	texCoords[1] = multiTexCoord1;
	texCoords[2] = multiTexCoord2;
	texCoords[3] = multiTexCoord3;
	texCoords[4] = multiTexCoord4;
	texCoords[5] = multiTexCoord5;
	texCoords[6] = multiTexCoord6;
	texCoords[7] = multiTexCoord7;
	texCoords[8] = multiTexCoord8;

	N = normalize( mul(normalMatrix, n) );

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = float3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition[0].xyz;

	A = float4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale );
	C = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vertexColor);
	D = sqrt( float4(lightSourceDiffuse[0].rgb, brightnessScale) );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float3 normalVector : TEXCOORD2;
    float4 boneWeights[2] : TEXCOORD5;
    float2 multiTexCoord0 : TEXCOORD7;
    float2 multiTexCoord1 : TEXCOORD8;
    float2 multiTexCoord2 : TEXCOORD9;
    float2 multiTexCoord3 : TEXCOORD10;
    float2 multiTexCoord4 : TEXCOORD11;
    float2 multiTexCoord5 : TEXCOORD12;
    float2 multiTexCoord6 : TEXCOORD13;
    float2 multiTexCoord7 : TEXCOORD14;
    float2 multiTexCoord8 : TEXCOORD15;
};

struct SPIRV_Cross_Output
{
    float3 LightDir : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    float2 texCoords[9] : TEXCOORD2;
    nointerpolation float4 A : TEXCOORD11;
    float4 C : TEXCOORD12;
    nointerpolation float4 D : TEXCOORD13;
    float3 N : TEXCOORD14;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    normalVector = stage_input.normalVector;
    multiTexCoord0 = stage_input.multiTexCoord0;
    multiTexCoord1 = stage_input.multiTexCoord1;
    multiTexCoord2 = stage_input.multiTexCoord2;
    multiTexCoord3 = stage_input.multiTexCoord3;
    multiTexCoord4 = stage_input.multiTexCoord4;
    multiTexCoord5 = stage_input.multiTexCoord5;
    multiTexCoord6 = stage_input.multiTexCoord6;
    multiTexCoord7 = stage_input.multiTexCoord7;
    multiTexCoord8 = stage_input.multiTexCoord8;
    boneWeights = stage_input.boneWeights;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.LightDir = LightDir;
    stage_output.ViewDir = ViewDir;
    stage_output.texCoords = texCoords;
    stage_output.A = A;
    stage_output.C = C;
    stage_output.D = D;
    stage_output.N = N;
    return stage_output;
}
