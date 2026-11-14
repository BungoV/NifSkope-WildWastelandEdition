// HLSL source of features/weather/cell_precip.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* lane WEATHERLOOK1 phase B: one precipitation particle a point (src/features/weather/cellprecip.cpp; the field is
 * src/features/weather/precipmath.h). World -> view by pcRow (rows of the view transform, scale in); the quad is built in
 * cell_precip.geom, flat-on to the camera. */

static float3 vertexPosition;	// world
static float4 vertexColor;	// rgb tint, a opacity (a far dot's thinning already in)
static float2 vertexUV;		// the atlas cell's corner (uv)
static float vertexAngle;	// spin about the view axis, radians
static float particleSize;	// the quad's half size, world units
static float3 vertexVelocity;	// world units / s
cbuffer LooseUniforms : register(b31, space0)
{
	float4 pcRow[3];
};


static float3 vsView;
static float4 vsColor;
static float2 vsCell;
static float vsAngle;
static float vsHalf;
static float3 vsVel;

void vert_main()
{
	float4 w = float4( vertexPosition, 1.0 );
	vsView = float3( dot( pcRow[0], w ), dot( pcRow[1], w ), dot( pcRow[2], w ) );
	vsVel = float3( dot( pcRow[0].xyz, vertexVelocity ), dot( pcRow[1].xyz, vertexVelocity ), dot( pcRow[2].xyz, vertexVelocity ) );
	vsHalf = particleSize * length( pcRow[0].xyz );
	vsColor = vertexColor;
	vsCell = vertexUV;
	vsAngle = vertexAngle;
	gl_Position = float4( vsView, 1.0 );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float2 vertexUV : TEXCOORD2;
    float vertexAngle : TEXCOORD3;
    float particleSize : TEXCOORD4;
    float3 vertexVelocity : TEXCOORD5;
};

struct SPIRV_Cross_Output
{
    float3 vsView : TEXCOORD0;
    float4 vsColor : TEXCOORD1;
    float2 vsCell : TEXCOORD2;
    float vsAngle : TEXCOORD3;
    float vsHalf : TEXCOORD4;
    float3 vsVel : TEXCOORD5;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vertexUV = stage_input.vertexUV;
    vertexAngle = stage_input.vertexAngle;
    particleSize = stage_input.particleSize;
    vertexVelocity = stage_input.vertexVelocity;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vsView = vsView;
    stage_output.vsColor = vsColor;
    stage_output.vsCell = vsCell;
    stage_output.vsAngle = vsAngle;
    stage_output.vsHalf = vsHalf;
    stage_output.vsVel = vsVel;
    return stage_output;
}
