// HLSL source of lookdev_bill.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

/* Lookdev sky billboard (lane PBRWX1), vertex stage: a camera-facing quad at a
 * world DIRECTION, at unit distance, half-size = tan(half-angle). */

#include "uniforms.src.hlsli"

static float3 vertexPosition;	// corners -1..1

uniform float3 centerWorld;	// unit, world (Z up)
uniform float halfSize;

static float2 uv;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float2 uv : TEXCOORD0;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	float3 c = mul(centerWorld, viewMatrix);
	float3 p = c + float3( vertexPosition.xy * halfSize, 0.0 );
	uv = float2( vertexPosition.x * 0.5 + 0.5, 0.5 - vertexPosition.y * 0.5 );
	float4 q = mul(float4( p, 1.0 ), projectionMatrix);
	gl_Position = float4( q.xy, 0.0, q.w );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.uv = uv;
    return stage_output;
}
