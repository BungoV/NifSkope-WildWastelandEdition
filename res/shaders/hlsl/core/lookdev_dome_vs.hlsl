// HLSL source of lookdev_dome.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

/* Lookdev weather sky dome (lane PBRWX1), vertex stage. The dome follows the
 * camera (the engine's sky root does): view ROTATION only, no translation.
 * Depth is off for every sky pass, so z is parked mid-range. */

#include "uniforms.src.hlsli"

static float3 vertexPosition;	// sky-root space, Z up
static float4 vertexColor;	// R = Horizon, G = Sky-Lower, B = Sky-Upper weights

static float4 vcol;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
};
struct SPIRV_Cross_Output
{
    float4 vcol : TEXCOORD0;
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	vcol = vertexColor;
	float4 q = mul(float4( mul(vertexPosition, viewMatrix), 1.0 ), projectionMatrix);
	gl_Position = float4( q.xy, 0.0, q.w );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexColor = stage_input.vertexColor;
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vcol = vcol;
    return stage_output;
}
