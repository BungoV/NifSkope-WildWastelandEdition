// HLSL source of features/post/game_doflens_prep.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
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
/* lane DOFLENS1: the thin-lens depth of field's first pass (src/features/post/cellpost.cpp). Quarter size: each texel q takes the
 * mean of the 4x4 full-size texels from 4q (four bilinear taps at the 2x2 block corners; Inf or NaN counts as 0) and
 * the signed circle of confusion (full px, game_doflens.glsl) of the NEAREST of four depths, so a near edge spreads.
 * t0 the frame, t1 the depth. */

Texture2D<float4> t0 : register(t1, space0); SamplerState _t0_sampler : register(s1, space0);
Texture2D<float4> t1 : register(t2, space0); SamplerState _t1_sampler : register(s2, space0);

#define WW_LOOSE_TYPES 1
#include "game_doflens.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "game_doflens.hlsli"
#undef WW_LOOSE_UNIFORMS
};
#include "game_doflens.hlsli"

static float4 fragColor;

void frag_main()
{
	int2 q = int2( gl_FragCoord.xy );
	float2 fs = float2( textureSize( t0, 0 ) );
	int2 hi = textureSize( t1, 0 ) - 1;
	float s1 = lensFocus( t1, _t1_sampler );
	float3 acc = (float3)( 0.0 );
	float zn = 1e30;
	bool sky = true;
	for ( int i = 0; i < 4; i++ ) {
		int2 o = int2( ( i & 1 ) * 2 + 1, ( i >> 1 ) * 2 + 1 );
		float2 pc = float2( q * 4 + o );
		float3 c = t0.Sample( _t0_sampler, pc / fs ).rgb;
		bool3 bad = ((asuint( c ) & 0x7f800000u) == (uint3)( 0x7f800000u ));
		acc += select(bad, (float3)( 0.0 ), c) * 0.25;
		float z = lensDist( t1.Load( int3(min( q * 4 + o, hi ), 0) ).r );
		if ( z >= 0.0 ) {
			sky = false;
			zn = min( zn, z );
		}
	}
	fragColor = float4( acc, lensCoc( sky ? -1.0 : zn, s1 ) );
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
