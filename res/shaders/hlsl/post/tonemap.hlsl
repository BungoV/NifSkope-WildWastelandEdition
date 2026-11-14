// Lane VKFRAME1 P6 (2026-11-12): the cell view's tone map on Vulkan, cell_hdr.frag's twin per sample. One thread a pixel
// walks its samples: the stencil test (hdrStencilTest / Ref) skips a sample the GL program discards (its output texel keeps
// what was there), else the mode's value goes to ToneOut at column x * samples + s, y (RGBA32F: the fragment's float output
// before the view's blend). Modes as cell_hdr.frag: 0 the cell's imagespace (dithered), 1 the value as written, 2 an effect
// over the sky by its coverage, 3 the bloom's glow at the backdrop's place on the curve. The Fusion Shaders bloom comes from
// its per-pixel combine (bloom_comb.hlsl), or under the fog bloom with Bloom Mip on, from the top's mips per sample
// (ToneFoBloomLod, lane VKFAR2). The per-sample math is
// ToneSample (ToneBindingsVk.hlsli), shared with the fragment pass tonemap_raster.hlsl.
// TONEMAP_RED skips the grading LUT (the gate's red must FAIL). TONEMAP_NO_WORK only loads each sample and stores it (the
// timing floor).
#include "ToneBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint w, h, S;
	ToneFrame.GetDimensions( w, h, S );
	if ( any( id.xy >= uint2( w, h ) ) )
		return;
	const int2 px = int2( id.xy );
#ifdef TONEMAP_NO_WORK
	for ( uint n = 0; n < S; n++ )
		ToneOut[uint2( id.x * S + n, id.y )] = ToneFrame.Load( px, n );
	return;
#endif
	const ToneJob j = ToneJobPc;
	F4FX_ImageSpace p = ToneImageSpace( j );
#ifdef TONEMAP_RED
	p.lutOn = false;
#endif
	// the Fusion Shaders bloom at this pixel times the bloom weight: the same at every sample (FOBLOOMLOD: per sample)
	float3 foBloom = float3( 0.0f, 0.0f, 0.0f );
	const bool lodOn = p.foOn && ( j.ints.x & TONE_F_FOBLOOMLOD ) != 0;
	if ( p.foOn && !lodOn )
		foBloom = ToneComb.Load( int3( px, 0 ) ).rgb * j.fo.x;
	for ( uint s = 0; s < S; s++ ) {
		float4 v;
		if ( lodOn )
			foBloom = ToneFoBloomLod( j, px, s );
		if ( ToneSample( j, p, px, s, foBloom, v ) )
			ToneOut[uint2( id.x * S + s, id.y )] = v;
	}
}
