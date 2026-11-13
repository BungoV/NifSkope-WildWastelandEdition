// HLSL source of uvedit.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
static float4 gl_Position;
static float2 texCoord;
cbuffer LooseUniforms : register(b31, space0)
{
	float4 viewScaleAndOffset;
};


static float2 multiTexCoord0;

void vert_main()
{
	float2	offs = multiTexCoord0;

	gl_Position = float4( offs * float2( 2.0, -2.0 ) + float2( -1.0, 1.0 ), (float2)( 1.0 ) );

	texCoord = offs * viewScaleAndOffset.xy + viewScaleAndOffset.zw;
}

struct SPIRV_Cross_Input
{
    float2 multiTexCoord0 : TEXCOORD7;
};

struct SPIRV_Cross_Output
{
    float2 texCoord : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    multiTexCoord0 = stage_input.multiTexCoord0;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.texCoord = texCoord;
    return stage_output;
}
