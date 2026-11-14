// HLSL source of features/post/cell_bloom_hold.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* MODULES1 R8-2: the bloom's level 0 held over the temporal jitter's period (src/features/post/cellbloom.cpp).
 * The chain reads the frame before the temporal resolve, so a bright thing under a pixel wide is hit by one jitter
 * and missed by the next: the whole halo blinks. While the camera stands still the level is the mean of its last
 * bloomHoldN frames (up to 8, the jitter's period), which is the same every frame. A moved camera counts from 1. */

Texture2DArray<float4> bloomRing : register(t0, space0); SamplerState _bloomRing_sampler : register(s0, space0);	// level 0 of the last 8 frames, one a layer
cbuffer LooseUniforms : register(b31, space0)
{
	int bloomHoldK;				// the newest layer
	int bloomHoldN;				// how many layers count, 1..8
};


static float4 fragColor;

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	int n = clamp( bloomHoldN, 1, 8 );
	float3 c = (float3)( 0.0 );
	for ( int i = 0; i < n; i++ )
		c += bloomRing.Load( int4(int3( p, ( bloomHoldK - i + 8 ) & 7 ), 0) ).rgb;
	fragColor = float4( c / float( n ), 1.0 );
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
