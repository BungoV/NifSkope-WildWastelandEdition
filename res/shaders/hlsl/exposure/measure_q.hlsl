// Lane VKFRAME1 P4 (2026-11-12): the exposure measure on Vulkan (cell_is_measure_q.frag): the shared linear frame's
// colour and stencil (bindings 5, 6, as bloom_src.hlsl reads them) into the quarter-size RGBA32F target (binding 7, the
// RGBA32F storage slot of the bloom layout). GL reads it back as before (cellhdr.cpp).
// One thread a frame pixel (16 x 16 a group = 4 x 4 quarter texels): each pixel's sample mean goes to group memory, then
// one thread a quarter texel adds its 16 pixels in the GL twin's order (bit-exact; one thread reading 4 x 4 x 4 samples
// was 0.143 ms against GL's 0.082 at 1920 x 1045). MEASURE_Q_RED skips the stencil test.
#define BLOOM_FRAME 1
#include "../post/BloomBindingsVk.hlsli"
#include "MeasureCommon.hlsli"

groupshared float4 gsPixel[16][16];

[numthreads( 16, 16, 1 )]
void main( uint3 id : SV_DispatchThreadID, uint3 lid : SV_GroupThreadID, uint3 gid : SV_GroupID )
{
	uint fw, fh, samples;
	BloomFrameColor.GetDimensions( fw, fh, samples );
	const int2 p = int2( id.xy );
#ifdef MEASURE_Q_RED
	gsPixel[lid.y][lid.x] = ( p.x < int( fw ) && p.y < int( fh ) ) ? float4( BloomFrameColor.Load( p, 0 ).rgb, 1.0 ) : float4( 0.0, 0.0, 0.0, -1.0 );
#else
	gsPixel[lid.y][lid.x] = F4FX_MeasurePixel( BloomFrameColor, BloomFrameStencil, p, int2( fw, fh ), int( samples ) );
#endif
	GroupMemoryBarrierWithGroupSync();
	if ( lid.x >= 4 || lid.y >= 4 )
		return;
	uint qw, qh;
	BloomComb.GetDimensions( qw, qh );
	const uint2 q = gid.xy * 4 + lid.xy;
	if ( q.x >= qw || q.y >= qh )
		return;
	float3 sum = float3( 0.0, 0.0, 0.0 );
	float cover = 0.0;
	for ( uint y = 0; y < 4; y++ )
		for ( uint x = 0; x < 4; x++ )
			F4FX_MeasureAdd( sum, cover, gsPixel[lid.y * 4 + y][lid.x * 4 + x] );
	BloomComb[q] = float4( sum / 16.0, cover / 16.0 );
}
