// HLSL source of pbr_route.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1
static bool gl_FrontFacing;

// PBR route debug view (lane PBRR1). The colour is chosen per shape on the CPU
// from the resolved route (Renderer::setupProgramRoute); this pass only lights
// it by N.L so the shape keeps its form.

uniform float3 routeColor;

static float3 LightDir;
static float3x3 btnMatrix;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    bool gl_FrontFacing : SV_IsFrontFace;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


void frag_main()
{
	float3 n = normalize( btnMatrix[2] );
	if ( !gl_FrontFacing )
		n = -n;
	float ndl = max( dot( n, normalize( LightDir ) ), 0.0 );
	fragColor = float4( routeColor * ( 0.4 + 0.6 * ndl ), 1.0 );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FrontFacing = stage_input.gl_FrontFacing;
    btnMatrix = stage_input.btnMatrix;
    LightDir = stage_input.LightDir;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
