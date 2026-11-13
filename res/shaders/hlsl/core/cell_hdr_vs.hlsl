// HLSL source of cell_hdr.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
// lane PERF2: the tone map's full-screen quad at the far plane (window depth 1.0, as cell_hdr_fd.frag wrote per
// sample): no depth written in the fragment, so the early stencil and depth tests skip the pixels a mode does not take

static float3 vertexPosition;

void vert_main()
{
	gl_Position = float4( vertexPosition.xy, 1.0, 1.0 );
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
