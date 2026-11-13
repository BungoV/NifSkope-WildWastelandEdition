// HLSL source of features/post/cell_lensflare.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane LIGHTS3 (LENSFLARE1): a lens-flare sprite, added to the tone-mapped frame (cell_lensflare.vert)

Texture2D<float4> flareTex : register(t0, space0); SamplerState _flareTex_sampler : register(s0, space0);

static float2 flareUV;
static float3 flareColor;
static float4 fragColor;

void frag_main()
{
	fragColor = float4( flareTex.Sample( _flareTex_sampler, flareUV ).rgb * flareColor, 0.0 );
}

struct SPIRV_Cross_Input
{
    float2 flareUV : TEXCOORD0;
    float3 flareColor : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    flareUV = stage_input.flareUV;
    flareColor = stage_input.flareColor;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
