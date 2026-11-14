// HLSL source of features/reflections/cell_reflcap.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "ReflectionsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* lane REFLPROBE1: a static shape into one face of a probe's G-buffer cube. Its own model-view matrix (as the
 * renderer draws it), view space -> world relative to the probe (rpRelFromView), then GL's face projection. */

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 rpRelFromView;
	row_major float4x4 rpClipFromRel;
	row_major float4x4 modelViewMatrix;
	float2 uvScale;
	float2 uvOffset;
};


static float3 vertexPosition;
static float4 vertexColor;
static float3 normalVector;
static float2 multiTexCoord0;

static float3 rpRel;
static float3 rpN;
static float4 rpC;
static float2 rpUv;

void vert_main()
{
	float4 rel = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), rpRelFromView);
	rpRel = rel.xyz;
	rpN = mul(( mul(normalVector, (float3x3)( modelViewMatrix )) ), (float3x3)( rpRelFromView ));
	rpC = vertexColor;
	rpUv = multiTexCoord0 * uvScale + uvOffset;
	gl_Position = mul(float4( rel.xyz, 1.0 ), rpClipFromRel);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float3 normalVector : TEXCOORD2;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float3 rpRel : TEXCOORD0;
    float3 rpN : TEXCOORD1;
    float4 rpC : TEXCOORD2;
    float2 rpUv : TEXCOORD3;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    normalVector = stage_input.normalVector;
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.rpRel = rpRel;
    stage_output.rpN = rpN;
    stage_output.rpC = rpC;
    stage_output.rpUv = rpUv;
    return stage_output;
}
