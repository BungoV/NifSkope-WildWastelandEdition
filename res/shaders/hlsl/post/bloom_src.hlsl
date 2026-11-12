// Lane VKFRAME1 P3 (2026-11-12): the bloom's source on Vulkan (cell_bloom_src.frag + its resolve blit): every sample of the
// linear frame whose stencil says "cell-lit" (& 3 == 1), times the exposure, rounded to half as GL's multisample target
// stores it, then the mean of the samples (the resolve) rounded to half (nearest even). Writes the chain's source (RGBA16F, the frame's size).
// params: exposure, impulse x, impulse y, impulse value (> 0: the test probe, no stencil); ints.x: 1 = the cell is on.
// BLOOM_SRC_RED skips the stencil test.
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
	const float4 p = BloomJobPc.params;
	const int2 xy = int2( id.xy );
	float4 c = float4( 0.0, 0.0, 0.0, 0.0 );
	if ( p.w > 0.0 ) {
		c = F4FX_BloomImpulse( xy, p.yz, p.w );
	} else {
		for ( uint s = 0; s < samples; s++ ) {
#ifdef BLOOM_SRC_RED
			const bool lit = true;
#else
			const bool lit = ( BloomFrameStencil.Load( xy, s ) & 3u ) == 1u;
#endif
			c += F4FX_BloomSourceSample( BloomFrameColor.Load( xy, s ).rgb, p.x, lit, BloomJobPc.ints.x != 0 );
		}
		c /= float( samples );
		c = f16tof32( f32tof16( c ) );	// nearest even here: a storage store of a float to half may truncate (measured, 1 ulp)
	}
	BloomTarget[id.xy] = c;
}
