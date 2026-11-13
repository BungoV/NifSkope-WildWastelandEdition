// HLSL source of ob_default.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
// this program's registers for blocks its includes declare (glslang numbers them per program and variant)
#ifndef WW_REGS_SET
#define WW_REGS_SET
#define WW_REG_globalUniforms b1
#endif
static float4 gl_Position;

static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 A;
static float4 C;
static float4 D;

static float3x3 reflMatrix;

#include "uniforms.src.hlsli"

uniform row_major float3x3 normalMatrix;			// in row-major order
uniform row_major float4x4 modelViewMatrix;

uniform float4 vertexColorOverride;	// components greater than zero replace the vertex color

static float3	vertexPosition;
static float4	vertexColor;
static float3	normalVector;
static float3	tangentVector;
static float3	bitangentVector;
static float2	multiTexCoord0;

#include "bonetransform.src.hlsli"

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
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
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(9) float3x3 reflMatrix : TEXCOORD9;
    float4 gl_Position : SV_Position;
};


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

	// NOTE: b<->t
	float3x3 btnMatrix = float3x3( normalize(mul(normalMatrix, b)), normalize(mul(normalMatrix, t)), normalize(mul(normalMatrix, n)) );
	btnMatrix = transpose( btnMatrix );

	reflMatrix = mul(envMapRotation, btnMatrix);

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = mul(float3(0.0, 0.0, 1.0), btnMatrix);	// orthographic view
	else
		ViewDir = mul(-v.xyz, btnMatrix);
	LightDir = mul(lightSourcePosition[0].xyz, btnMatrix);

	A = float4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale );
	C = select((vertexColorOverride > (float4)(0.0)), vertexColorOverride, vertexColor);
	D = float4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    boneWeights = stage_input.boneWeights;
    vertexPosition = stage_input.vertexPosition;
    normalVector = stage_input.normalVector;
    tangentVector = stage_input.tangentVector;
    bitangentVector = stage_input.bitangentVector;
    multiTexCoord0 = stage_input.multiTexCoord0;
    vertexColor = stage_input.vertexColor;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.texCoord = texCoord;
    stage_output.reflMatrix = reflMatrix;
    stage_output.ViewDir = ViewDir;
    stage_output.LightDir = LightDir;
    stage_output.A = A;
    stage_output.C = C;
    stage_output.D = D;
    return stage_output;
}
