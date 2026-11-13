// HLSL source of features/shadows/lookdev_csmdepth_at.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
/* The cascade caster pass, alpha-tested shapes (lane VOLAIR1): the same test the colour pass
 * runs (fo4_default.frag: baseMap.a * alpha against alphaThreshold), kept where it passes. */

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float2 uvScale;
	float2 uvOffset;
	float alpha;
	float alphaThreshold;
};


static float2 texCoord;

void frag_main()
{
	if ( BaseMap.Sample( _BaseMap_sampler, texCoord * uvScale + uvOffset ).a * alpha < alphaThreshold )
		discard;
}

struct SPIRV_Cross_Input
{
    float2 texCoord : TEXCOORD0;
};

void main(SPIRV_Cross_Input stage_input)
{
    texCoord = stage_input.texCoord;
    frag_main();
}
