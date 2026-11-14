// HLSL source of pbr_route.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static bool gl_FrontFacing;
// PBR route debug view (lane PBRR1). The colour is chosen per shape on the CPU
// from the resolved route (Renderer::setupProgramRoute); this pass only lights
// it by N.L so the shape keeps its form.

cbuffer LooseUniforms : register(b31, space0)
{
	float3 routeColor;
};


static float3 LightDir;
static float3x3 btnMatrix;

static float4 fragColor;

void frag_main()
{
	float3 n = normalize( btnMatrix[2] );
	if ( !gl_FrontFacing )
		n = -n;
	float ndl = max( dot( n, normalize( LightDir ) ), 0.0 );
	fragColor = float4( routeColor * ( 0.4 + 0.6 * ndl ), 1.0 );
}

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

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    LightDir = stage_input.LightDir;
    btnMatrix = stage_input.btnMatrix;
    gl_FrontFacing = stage_input.gl_FrontFacing;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
