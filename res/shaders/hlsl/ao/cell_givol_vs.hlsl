// HLSL source of features/ao/cell_givol.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static int gl_VertexID;
static int gl_InstanceID;
/* lane PERF1: one GI volume shape per instance, its hull (a box grown by the mask's bias) from gl_VertexID, 36
 * vertices, outward faces counter-clockwise; drawn back faces only (src/gl/celllights.cpp giVolMaskPass) */

#include "cell_givol.hlsli"

cbuffer LooseUniforms : register(b31, space0)
{
	float4 volClip[4];	// clip = volClip[r] . (world, 1), r = x, y, z, w
	float volGrow;		// game units
};


static int vShape;

static const float3 FN[6] = { float3( 1, 0, 0 ), float3( -1, 0, 0 ), float3( 0, 1, 0 ), float3( 0, -1, 0 ), float3( 0, 0, 1 ), float3( 0, 0, -1 ) };
static const float3 FU[6] = { float3( 0, 1, 0 ), float3( 0, 0, 1 ), float3( 0, 0, 1 ), float3( 1, 0, 0 ), float3( 1, 0, 0 ), float3( 0, 1, 0 ) };
static const float3 FV[6] = { float3( 0, 0, 1 ), float3( 0, 1, 0 ), float3( 1, 0, 0 ), float3( 0, 0, 1 ), float3( 0, 1, 0 ), float3( 1, 0, 0 ) };
static const float2 FC[4] = { float2( -1, -1 ), float2( 1, -1 ), float2( 1, 1 ), float2( -1, 1 ) };

void vert_main()
{
	int s = gl_InstanceID;
	vShape = s;
	float4 a = volShapes.Load( 3 * s );
	float4 b = volShapes.Load( 3 * s + 1 );
	float4 c = volShapes.Load( 3 * s + 2 );
	int f = gl_VertexID / 6, k = gl_VertexID % 6;
	float2 q = FC[k < 3 ? k : k == 3 ? 0 : k - 1];
	float3 l = 0.5 * ( FN[f] + q.x * FU[f] + q.y * FV[f] ) * ( b.xyz + 2.0 * volGrow );
	float4 w = float4( mul(l, volRot( c.xyz )) + a.xyz, 1.0 );
	gl_Position = float4( dot( volClip[0], w ), dot( volClip[1], w ), dot( volClip[2], w ), dot( volClip[3], w ) );
}

struct SPIRV_Cross_Input
{
    uint gl_VertexID : SV_VertexID;
    uint gl_InstanceID : SV_InstanceID;
};

struct SPIRV_Cross_Output
{
    nointerpolation int vShape : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_VertexID = int(stage_input.gl_VertexID);
    gl_InstanceID = int(stage_input.gl_InstanceID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vShape = vShape;
    return stage_output;
}
