// HLSL source of features/post/cell_bloom_comb.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane PERFFIX3 (item 8): the FO4CS bloom's final tent, once per PIXEL (src/features/post/cellbloom.cpp combine). cell_hdr.frag
 * runs per sample, but the tent's uv is ( floor( gl_FragCoord ) + 0.5 ) / frame for every sample of a pixel, so its
 * 9 (27 with the glare's dispersion) bilinear taps gave the same value up to 4 times at 4x MSAA. This pass writes that
 * tent at its pixel into a full-size RGBA32F target (the same expression, the same uniforms' names and values), and the
 * tone map reads it with one texelFetch. It runs only while the tent's lod is the same at every sample of the frame
 * (no Fog Bloom Boost per sample under Bloom Mip): alpha holds the lod it used. cellbloom.cpp runs it only with the
 * glare's dispersion (27 taps): the plain 9-tap tent per sample costs about one RGBA32F fetch per sample (measured,
 * tests/features/post/bloomcomb). Exactness gate: tests/features/post/bloomcomb/bloomcomb_check.py. */

#include "cell_bloom.hlsli"

Texture2D<float4> cellIsFoBloom : register(t0, space0); SamplerState _cellIsFoBloom_sampler : register(s0, space0);	// the chain's top (the tone map's own sampler, unit, filter and mips)
cbuffer LooseUniforms : register(b31, space0)
{
	float4 cellIsFoBloomParams;	// radius (pixels), bloom weight, dispersion, the Bloom Mip mode (cell_hdr.frag)
	float2 cellIsFoBloomFrame;	// the frame's size
};


static float4 fragColor;

void frag_main()
{
	float2 uv = ( floor( gl_FragCoord.xy ) + 0.5 ) / cellIsFoBloomFrame;
	// cell_hdr.frag's bloomLod with the gain at 1 (no fog bloom per sample): 2 for the red wide, else 0
	float bloomLod = cellIsFoBloomParams.w < 0.0 ? 2.0 : 0.0;
	if ( bloomLod > 0.0 )
		fragColor = float4( bloomTentDispersedLod( cellIsFoBloom, _cellIsFoBloom_sampler, uv, cellIsFoBloomParams.x, cellIsFoBloomParams.z, bloomLod ),
			bloomLod );
	else
		fragColor = float4( bloomTentDispersed( cellIsFoBloom, _cellIsFoBloom_sampler, uv, cellIsFoBloomParams.x, cellIsFoBloomParams.z ), 0.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
