// Lane VKFRAME1 P2 (2026-11-12): the bloom's final tent once per pixel on Vulkan (cell_bloom_comb.frag's twin, lod 0): the
// dispersed 9-tap tent of the chain's top (BloomSource) at the pixel's centre, into a full-size RGBA32F target the tone map
// fetches per sample; alpha 0 (the lod it used). params: radius (pixels), bloom weight (unused here: the tone map applies
// it), dispersion. BLOOM_COMB_RED drops the dispersion.
#define BLOOM_FRAME 1
#include "BloomBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint w, h;
	BloomComb.GetDimensions( w, h );
	if ( any( id.xy >= uint2( w, h ) ) )
		return;
	uint mw, mh;
	BloomSource.GetDimensions( mw, mh );
	const float2 texel = 1.0 / float2( max( uint2( mw, mh ), uint2( 1, 1 ) ) );
	const float2 uv = ( float2( id.xy ) + 0.5 ) / float2( max( uint2( w, h ), uint2( 1, 1 ) ) );
	const float4 p = BloomJobPc.params;
#ifdef BLOOM_COMB_RED
	const float dispersion = 0.0;
#else
	const float dispersion = p.z;
#endif
	BloomComb[id.xy] = float4( F4FX_UpsampleTentDispersed( BloomSource, BloomLinearClamp, uv, texel, p.x, dispersion ), 0.0 );
}
