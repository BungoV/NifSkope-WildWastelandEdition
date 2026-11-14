// HLSL source of features/post/cell_bloomblur.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
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
/* lane PERFFIX1: the vanilla bloom's blur on the GPU. The CPU twin is celllights.cpp wwCellImageSpaceSetBloom (the red
 * WW_CELL_BLOOM_GPU_RED=1, and the numpy gate notes/perffix1/bloom): the game's radius-7 blur, 15 taps
 * exp( -2 x^2 / 49 ) normalized (bloomW, from the CPU so both sides use the same floats), clamped at the edges. Pass 1
 * (bloomVertical 1) reads the measure's quarter-size level (bloomLod 2: rgb = the mean light over the texel, a
 * non-finite value read as 0), takes the bright pass scale x max( 0, c - threshold ) per tap and sums down the
 * column; pass 2 sums its output across the row. */

Texture2D<float4> bloomSrc : register(t0, space0); SamplerState _bloomSrc_sampler : register(s0, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	int bloomLod;
	int bloomVertical;
	float2 bloomPass;	// threshold, scale (the imagespace HDR data's 3rd and 4th floats)
	float bloomW[15];
};


static float4 fragColor;

float finiteOr0( float v )
{
	return or( isnan( v ), isinf( v ) ) ? 0.0 : v;
}

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	int2 sz = textureSize( bloomSrc, bloomLod );
	float3 a = (float3)( 0.0 );
	for ( int k = -7; k <= 7; k++ ) {
		int2 q = bloomVertical == 1 ? int2( p.x, clamp( p.y + k, 0, sz.y - 1 ) ) : int2( clamp( p.x + k, 0, sz.x - 1 ), p.y );
		float3 v = bloomSrc.Load( int3(q, bloomLod) ).rgb;
		if ( bloomVertical == 1 )
			v = bloomPass.y * max( (float3)( 0.0 ), float3( finiteOr0( v.r ), finiteOr0( v.g ), finiteOr0( v.b ) ) - bloomPass.x );
		a += bloomW[k + 7] * v;
	}
	fragColor = float4( a, 1.0 );
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
