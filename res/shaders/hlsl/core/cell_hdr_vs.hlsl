// HLSL source of cell_hdr.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_VS 1
static float4 gl_Position;

// lane PERF2: the tone map's full-screen quad at the far plane (window depth 1.0, as cell_hdr_fd.frag wrote per
// sample): no depth written in the fragment, so the early stencil and depth tests skip the pixels a mode does not take

static float3 vertexPosition;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
};
struct SPIRV_Cross_Output
{
    float4 gl_Position : SV_Position;
};


void vert_main()
{
	gl_Position = float4( vertexPosition.xy, 1.0, 1.0 );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
