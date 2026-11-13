// HLSL source of features/clouds/cloudmap.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
/* lane CLOUD1: one weather cloud layer projected from the dome onto its band's plane (src/features/clouds/cloudmap.cpp).
 * The Clouds.nif vertex is a direction from the eye; the plane at height cmHeight above the eye is met at
 * dir.xy * cmHeight / dir.z. w = dir.z makes that a perspective divide (uv interpolates correctly on the plane) and
 * clips everything at or under the horizon. show: a quad in the corner of the screen showing the map. */

static float3 vertexPosition;
static float4 vertexColor;	// alpha = the layer's edge fade
static float2 multiTexCoord0;
cbuffer LooseUniforms : register(b31, space0)
{
	float cmHeight;	// the band plane above the eye, game units
	float cmHalfExtent;	// the map square's half side, game units
	float2 uvOffset;	// the layer's scroll
	bool cmShow;
	bool cmQuad;	// SKYLIGHT1 round 3: the layer laid on the world (a quad over the map, uv from the world point)
};


static float vfade;
static float2 uv;

void vert_main()
{
	if ( cmShow ) {
		vfade = 1.0;
		uv = vertexPosition.xy * 0.5 + 0.5;
		gl_Position = float4( vertexPosition.xy, 0.0, 1.0 );
		return;
	}
	if ( cmQuad ) {
		vfade = vertexColor.a;
		uv = multiTexCoord0 + uvOffset;
		gl_Position = float4( vertexPosition.xy, 0.0, 1.0 );
		return;
	}
	vfade = vertexColor.a;
	uv = multiTexCoord0 + uvOffset;
	gl_Position = float4( vertexPosition.xy * ( cmHeight / cmHalfExtent ), 0.0, vertexPosition.z );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float vfade : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vfade = vfade;
    stage_output.uv = uv;
    return stage_output;
}
