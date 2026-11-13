// HLSL source of lookdev_bill.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* Lookdev sky billboard (lane PBRWX1), vertex stage: a camera-facing quad at a
 * world DIRECTION, at unit distance, half-size = tan(half-angle). */

#include "uniforms.hlsli"

static float3 vertexPosition;	// corners -1..1
cbuffer LooseUniforms : register(b31, space0)
{
	float3 centerWorld;	// unit, world (Z up)
	float halfSize;
};


static float2 uv;

void vert_main()
{
	float3 c = mul(centerWorld, viewMatrix);
	float3 p = c + float3( vertexPosition.xy * halfSize, 0.0 );
	uv = float2( vertexPosition.x * 0.5 + 0.5, 0.5 - vertexPosition.y * 0.5 );
	float4 q = mul(float4( p, 1.0 ), projectionMatrix);
	gl_Position = float4( q.xy, 0.0, q.w );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float2 uv : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.uv = uv;
    return stage_output;
}
