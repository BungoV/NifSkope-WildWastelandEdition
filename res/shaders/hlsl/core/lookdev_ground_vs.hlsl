// HLSL source of lookdev_ground.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* Lookdev ground plane (lane PBRR2B), vertex stage. The quad arrives in the
 * scene's world space (Z up); texture coordinates come from world XY. */

#include "uniforms.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 modelViewMatrix;
};


static float3 vertexPosition;

static float3 worldPos;
static float3 viewPos;	// lane FOG1: the fog reads the view-space position

void vert_main()
{
	worldPos = vertexPosition;
	float4 v = mul(float4( vertexPosition, 1.0 ), modelViewMatrix);
	viewPos = v.xyz;
	gl_Position = mul(v, projectionMatrix);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float3 worldPos : TEXCOORD0;
    float3 viewPos : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.worldPos = worldPos;
    stage_output.viewPos = viewPos;
    return stage_output;
}
