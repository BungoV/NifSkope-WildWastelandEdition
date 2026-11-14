// HLSL source of features/post/cell_bloom_down.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
// FO4CS's BloomDownCS as a fragment pass, one mip of the pyramid (src/features/post/cellbloom.h)

#include "cell_bloom.hlsli"

Texture2D<float4> bloomSrc : register(t0, space0); SamplerState _bloomSrc_sampler : register(s0, space0);		// the resolved source (level 0) or the previous mip, linear, clamped
cbuffer LooseUniforms : register(b31, space0)
{
	float2 bloomDst;			// this mip's size
	float4 bloomParams;		// threshold, soft knee, prefilter (level 0), scale (1)
	int bloomRed;			// 1: level 0 a plain box, no Karis average
};


static float4 fragColor;

// F4FX_BloomThresholdWeight
float bloomThresholdWeight( float luminance, float threshold, float softKnee )
{
	luminance = max( luminance, 0.0 );
	if ( luminance <= 0.0001 )
		return 0.0;
	threshold = max( threshold, 0.0 );
	float knee = threshold * max( softKnee, 0.0 );
	float hard = luminance - threshold;
	float contribution = max( hard, 0.0 );
	if ( knee > 0.0001 ) {
		float soft = clamp( hard + knee, 0.0, 2.0 * knee );
		contribution = max( contribution, ( soft * soft ) / ( 4.0 * knee ) );
	}
	return clamp( contribution / luminance, 0.0, 1.0 );
}

void frag_main()
{
	float2 uv = ( floor( gl_FragCoord.xy ) + 0.5 ) / bloomDst;
	float2 texel = 1.0 / float2( max( textureSize( bloomSrc, 0 ), (int2)( 1 ) ) );
	float prefilter = clamp( bloomParams.z, 0.0, 1.0 );
	float karis = bloomRed == 1 ? 0.0 : prefilter;
	float3 taps[4];
	taps[0] = bloomSanitize( bloomSrc.SampleLevel( _bloomSrc_sampler, uv + texel * float2( -0.5, -0.5 ), 0.0 ).rgb );
	taps[1] = bloomSanitize( bloomSrc.SampleLevel( _bloomSrc_sampler, uv + texel * float2( 0.5, -0.5 ), 0.0 ).rgb );
	taps[2] = bloomSanitize( bloomSrc.SampleLevel( _bloomSrc_sampler, uv + texel * float2( -0.5, 0.5 ), 0.0 ).rgb );
	taps[3] = bloomSanitize( bloomSrc.SampleLevel( _bloomSrc_sampler, uv + texel * float2( 0.5, 0.5 ), 0.0 ).rgb );
	float3 color = (float3)( 0.0 );
	float weightSum = 0.0;
	for ( int t = 0; t < 4; t++ ) {
		float w = lerp( 1.0, 1.0 / ( 1.0 + bloomLuma( taps[t] ) ), karis );
		color += taps[t] * w;
		weightSum += w;
	}
	color /= max( weightSum, 0.0001 );
	float extract = lerp( 1.0, bloomThresholdWeight( bloomLuma( color ), bloomParams.x, bloomParams.y ), prefilter );
	fragColor = float4( color * extract * max( bloomParams.w, 0.0 ), 1.0 );
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
