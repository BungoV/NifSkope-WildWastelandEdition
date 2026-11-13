// HLSL source of features/far-lod/cell_fardots.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "FarLodBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* One bulb a point, spread over a normalized Gaussian of sigma = max(0.6 px, projected bulb size): a pixel shows
 * I x (f / d)^2 x G / (2 pi sigma^2), so the dot's screen sum is the light reaching the camera. */

static float3 vertexPosition;	// world
static float3 vertexColor;	// the bulb's intensity I (the frame's linear units x area)
cbuffer LooseUniforms : register(b31, space0)
{
	float4 dotRow[3];
	row_major float4x4 dotProj;
	float3 dotCam;	// the camera, world
	float2 dotBand;
	float dotFocal;	// f, pixels
	float dotBulb;	// the bulb radius, world units
	int dotRed;	// 1 = never faded (debug)
};


static float3 dotValue;
static float3 dotView;	// lane VOLAIR1: the bulb in view space, for the air between it and the camera
static float dotSigma;

void vert_main()
{
	float4 w = float4( vertexPosition, 1.0 );
	float3 v = float3( dot( dotRow[0], w ), dot( dotRow[1], w ), dot( dotRow[2], w ) );
	gl_Position = mul(float4( v, 1.0 ), dotProj);
	dotView = v;
	float d = max( length( vertexPosition - dotCam ), 1.0 );
	float fade = ( dotRed & 1 ) != 0 ? 1.0 : smoothstep( dotBand.x, dotBand.y, d );
	dotSigma = max( 0.6, dotFocal * dotBulb / d );
	float k = dotFocal / d;
	dotValue = fade * vertexColor * k * k / ( 6.2831853 * dotSigma * dotSigma );
	gl_PointSize = fade > 0.0 ? min( 2.0 * ceil( 3.0 * dotSigma ) + 1.0, 63.0 ) : 0.0;
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float3 vertexColor : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float3 dotValue : TEXCOORD0;
    float3 dotView : TEXCOORD1;
    float dotSigma : TEXCOORD2;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.dotValue = dotValue;
    stage_output.dotView = dotView;
    stage_output.dotSigma = dotSigma;
    return stage_output;
}
