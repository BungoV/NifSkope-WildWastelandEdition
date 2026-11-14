// HLSL source of features/shadows/lookdev_csmremap.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static int gl_VertexID;
// lane SHADOWCACHE1: one triangle over the viewport, no vertex data
void vert_main()
{
	float2 p = float2( ( gl_VertexID << 1 ) & 2, gl_VertexID & 2 );
	gl_Position = float4( p * 2.0 - 1.0, 0.0, 1.0 );
}

struct SPIRV_Cross_Input
{
    uint gl_VertexID : SV_VertexID;
};

struct SPIRV_Cross_Output
{
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_VertexID = int(stage_input.gl_VertexID);
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
