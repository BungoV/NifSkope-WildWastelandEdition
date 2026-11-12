// Lane VKBLOOM1 (2026-11-13): the backdrop mode 3 reads on Vulkan (cellhdr.cpp backdropCopy: the view's RGBA8 samples
// resolved by a blit into RGBA16F). The mean of every sample of the view's copy, rounded to half (nearest even), as GL's
// resolve keeps it; an RGBA8 resolve rounds the mean to 8 bits, which the vanilla curve's shoulder turned into 199 levels.
// Reads binding 5 (the view's copy, multisampled), writes the backdrop (RGBA16F, the view's size).
#define BLOOM_FRAME 1
#include "BloomBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !BloomTexel( id, size ) )
		return;
	uint fw, fh, samples;
	BloomFrameColor.GetDimensions( fw, fh, samples );
	samples = max( samples, 1u );
	const int2 xy = int2( id.xy );
	float4 c = float4( 0.0, 0.0, 0.0, 0.0 );
	for ( uint s = 0; s < samples; s++ )
		c += BloomFrameColor.Load( xy, s );
	c /= float( samples );
	BloomTarget[id.xy] = f16tof32( f32tof16( c ) );
}
