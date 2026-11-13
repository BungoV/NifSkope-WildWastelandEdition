// HLSL source of features/lights/cell_shadowdepth.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "LightsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* Shadow caster vertex stage: model-view, then view -> light-relative world (shRelFromView), then the
 * cube face's projection (shClipFromRel: GL's face axes, 90 degrees). */

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 shRelFromView;
	row_major float4x4 shClipFromRel;
	row_major float4x4 modelViewMatrix;
};


static float3 vertexPosition;
static float2 multiTexCoord0;	// lane GAPFIX1: alpha-tested casters' first UV set

static float3 shRel;
static float2 texCoord;

void vert_main()
{
	float4 rel = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), shRelFromView);
	shRel = rel.xyz;
	texCoord = multiTexCoord0;
	gl_Position = mul(float4( rel.xyz, 1.0 ), shClipFromRel);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float3 shRel : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.shRel = shRel;
    stage_output.texCoord = texCoord;
    return stage_output;
}
