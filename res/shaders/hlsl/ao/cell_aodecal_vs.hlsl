// HLSL source of features/ao/cell_aodecal.vert (vertex shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
// model space straight to clip space (model -> view -> projection composed on the CPU)

static float3 vertexPosition;
cbuffer LooseUniforms : register(b31, space0)
{
	float4 aoClip[4];		// clip = aoClip[r] . (model, 1), r = x, y, z, w
};


void vert_main()
{
	float4 p = float4( vertexPosition, 1.0 );
	gl_Position = float4( dot( aoClip[0], p ), dot( aoClip[1], p ), dot( aoClip[2], p ), dot( aoClip[3], p ) );
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
