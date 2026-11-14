// HLSL source of sk_default.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
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

static float3x3 btnMatrix;

static float4 A;
static float4 C;
static float4 D;

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
static float3 tangentVector;
static float3 bitangentVector;
static float2 multiTexCoord0;

#include "bonetransform.hlsli"

void vert_main()
{
	float4	v = float4( vertexPosition, 1.0 );
	float3	n = normalVector;
	float3	t = tangentVector;
	float3	b = bitangentVector;

	if ( boneWeights[0].x > 0.0 && doSkinning )
		boneTransform( v, n, t, b );

	v = mul(v, modelViewMatrix);
	gl_Position = mul(v, projectionMatrix);
	texCoord = multiTexCoord0;

	btnMatrix[2] = normalize( mul(normalMatrix, n) );
	btnMatrix[1] = normalize( mul(normalMatrix, t) );
	btnMatrix[0] = normalize( mul(normalMatrix, b) );

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
    float3 normalVector : TEXCOORD2;
    float3 tangentVector : TEXCOORD3;
    float3 bitangentVector : TEXCOORD4;
    float4 boneWeights[2] : TEXCOORD5;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    normalVector = stage_input.normalVector;
    tangentVector = stage_input.tangentVector;
    bitangentVector = stage_input.bitangentVector;
    multiTexCoord0 = stage_input.multiTexCoord0;
    boneWeights = stage_input.boneWeights;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.LightDir = LightDir;
    stage_output.ViewDir = ViewDir;
    stage_output.texCoord = texCoord;
    stage_output.btnMatrix = btnMatrix;
    stage_output.A = A;
    stage_output.C = C;
    stage_output.D = D;
    return stage_output;
}
