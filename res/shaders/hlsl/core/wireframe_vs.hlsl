// HLSL source of wireframe.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static int gl_VertexID;
static float3x3 btnMatrix;

static float4 vsColor;

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float3x3 normalMatrix;			// in row-major order
	row_major float4x4 modelViewMatrix;
	float4 vertexColorOverride;	// components greater than zero replace the vertex color
	float4 highlightColor;
	int selectionParam;			// vertex selected (-1: none)
};


static float3 vertexPosition;
static float4 vertexColor;
static float3 normalVector;
static float3 tangentVector;
static float3 bitangentVector;

#include "bonetransform.hlsli"

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
		vsColor = select((vertexColorOverride > (float4)( 0.0 )), vertexColorOverride, vertexColor);
}

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

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    normalVector = stage_input.normalVector;
    tangentVector = stage_input.tangentVector;
    bitangentVector = stage_input.bitangentVector;
    boneWeights = stage_input.boneWeights;
    gl_VertexID = int(stage_input.gl_VertexID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.btnMatrix = btnMatrix;
    stage_output.vsColor = vsColor;
    return stage_output;
}
