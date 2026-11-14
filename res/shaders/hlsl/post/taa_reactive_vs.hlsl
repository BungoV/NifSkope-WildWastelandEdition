// HLSL source of features/post/taa_reactive.vert (vertex shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
// lane TAAINPUTS1: a see-through shape or a particle system's sprites at the frame's (jittered) clip position

cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 clipJit;
};


static float3 vertexPosition;
static float4 vertexColor;
static float2 multiTexCoord0;

static float4 vColor;
static float2 texCoord;

void vert_main()
{
	vColor = vertexColor;
	texCoord = multiTexCoord0;
	gl_Position = mul(float4( vertexPosition, 1.0 ), clipJit);
}

struct SPIRV_Cross_Input
{
    float3 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float4 vColor : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
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
    stage_output.vColor = vColor;
    stage_output.texCoord = texCoord;
    return stage_output;
}
