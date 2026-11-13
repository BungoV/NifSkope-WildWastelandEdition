// HLSL source of lookdev_dome.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* Lookdev weather sky dome (lane PBRWX1), vertex stage. The dome follows the
 * camera (the engine's sky root does): view ROTATION only, no translation.
 * Depth is off for every sky pass, so z is parked mid-range. */

#include "uniforms.hlsli"

static float3 vertexPosition;	// sky-root space, Z up
static float4 vertexColor;	// R = Horizon, G = Sky-Lower, B = Sky-Upper weights

static float4 vcol;

void vert_main()
{
	vcol = vertexColor;
	float4 q = mul(float4( mul(vertexPosition, viewMatrix), 1.0 ), projectionMatrix);
	gl_Position = float4( q.xy, 0.0, q.w );
}

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

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vcol = vcol;
    return stage_output;
}
