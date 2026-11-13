// HLSL source of wireframe.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
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
static int gl_VertexID;

static float3x3 btnMatrix;

static float4 vsColor;

#include "uniforms.src.hlsli"

uniform row_major float3x3 normalMatrix;			// in row-major order
uniform row_major float4x4 modelViewMatrix;

uniform float4 vertexColorOverride;	// components greater than zero replace the vertex color
uniform float4 highlightColor;
uniform int selectionParam;			// vertex selected (-1: none)

static float3	vertexPosition;
static float4	vertexColor;
static float3	normalVector;
static float3	tangentVector;
static float3	bitangentVector;

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
    uint gl_VertexID : SV_VertexID;
};
struct SPIRV_Cross_Output
{
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(0) float4 vsColor : TEXCOORD0;
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

	gl_Position = mul(v, modelViewMatrix);

	btnMatrix[2] = normalize( mul(normalMatrix, n) );
	btnMatrix[1] = normalize( mul(normalMatrix, t) );
	btnMatrix[0] = normalize( mul(normalMatrix, b) );

	if ( gl_VertexID == selectionParam )
		vsColor = highlightColor;
	else
		vsColor = select((vertexColorOverride > (float4)(0.0)), vertexColorOverride, vertexColor);
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_VertexID = int(stage_input.gl_VertexID);
    boneWeights = stage_input.boneWeights;
    vertexPosition = stage_input.vertexPosition;
    normalVector = stage_input.normalVector;
    tangentVector = stage_input.tangentVector;
    bitangentVector = stage_input.bitangentVector;
    vertexColor = stage_input.vertexColor;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.btnMatrix = btnMatrix;
    stage_output.vsColor = vsColor;
    return stage_output;
}
