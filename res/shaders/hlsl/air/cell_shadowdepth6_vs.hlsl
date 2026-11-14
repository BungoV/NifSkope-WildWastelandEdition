// HLSL source of features/air/cell_shadowdepth6.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "AirBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static int gl_InstanceID;
static uint gl_ViewportIndex;
/* lane MODULES1: the air lamp atlas's layered caster pass (src/features/air/airlampatlas.cpp). One instanced draw a caster:
 * instance i draws cube face shFaces[i] into viewport shFaces[i] (the face's atlas tile), with that face's projection.
 * The same math as cell_shadowdepth.vert, so a tile holds the same depths as the one-draw-a-face pass. */

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 shRelFromView;
	row_major float4x4 shClipFace[6];
	int shFaces[6];
	row_major float4x4 modelViewMatrix;
};


static float3 vertexPosition;
static float2 multiTexCoord0;

static float3 shRel;
static float2 texCoord;

void vert_main()
{
	int f = shFaces[gl_InstanceID];
	float4 rel = mul(( mul(float4( vertexPosition, 1.0 ), modelViewMatrix) ), shRelFromView);
	shRel = rel.xyz;
	texCoord = multiTexCoord0;
	gl_Position = mul(float4( rel.xyz, 1.0 ), shClipFace[f]);
	gl_ViewportIndex = f;
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float2 multiTexCoord0 : TEXCOORD7;
    uint gl_InstanceID : SV_InstanceID;
};

struct SPIRV_Cross_Output
{
    float3 shRel : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 gl_Position : SV_Position;
    uint gl_ViewportIndex : SV_ViewportArrayIndex;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    multiTexCoord0 = stage_input.multiTexCoord0;
    gl_InstanceID = int(stage_input.gl_InstanceID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.shRel = shRel;
    stage_output.texCoord = texCoord;
    stage_output.gl_ViewportIndex = gl_ViewportIndex;
    return stage_output;
}
