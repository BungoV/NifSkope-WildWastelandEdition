// HLSL source of features/post/cell_bloom_up.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* FO4CS's BloomUpCS as a fragment pass: source x params.y + tent(accumulated) x params.z, the tent's red and blue
 * pushed along the radial by the dispersion. */

#include "cell_bloom.hlsli"

Texture2D<float4> bloomSrc : register(t0, space0); SamplerState _bloomSrc_sampler : register(s0, space0);		// this level's mip (the source band)
Texture2D<float4> bloomMip : register(t1, space0); SamplerState _bloomMip_sampler : register(s1, space0);		// the accumulated chain below it
cbuffer LooseUniforms : register(b31, space0)
{
	float2 bloomDst;			// this level's size
	float4 bloomParams;		// radius (pixels), source weight, bloom weight, dispersion
};


static float4 fragColor;

void frag_main()
{
	float2 uv = ( floor( gl_FragCoord.xy ) + 0.5 ) / bloomDst;
	float3 source = bloomSanitize( bloomSrc.SampleLevel( _bloomSrc_sampler, uv, 0.0 ).rgb );
	float3 bloom = bloomTentDispersed( bloomMip, _bloomMip_sampler, uv, bloomParams.x, bloomParams.w );
	fragColor = float4( source * bloomParams.y + bloom * bloomParams.z, 1.0 );
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
