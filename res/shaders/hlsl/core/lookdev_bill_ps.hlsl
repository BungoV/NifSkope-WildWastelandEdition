// HLSL source of lookdev_bill.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
/* Lookdev sky billboard (lane PBRWX1): rgb = texture (sRGB-decoded here: the
 * sky DDS carry no DX10 header, so GL samples them raw) x tint (linear),
 * a = texture alpha x alpha. Blended in display space (no HDR yet, ruling). */

#include "uniforms.hlsli"

Texture2D<float4> Tex : register(t0, space0); SamplerState _Tex_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float3 tint;
	float alpha;
	float sceneExposure;
	int viewTransform;
};


static float2 uv;

static float4 fragColor;

#include "lookdev_output.hlsli"

void frag_main()
{
	float4 t = Tex.Sample( _Tex_sampler, uv );
	fragColor = float4( studioOutput( srgbToLinear( t.rgb ) * tint ), t.a * alpha );
}

struct SPIRV_Cross_Input
{
    float2 uv : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    uv = stage_input.uv;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
