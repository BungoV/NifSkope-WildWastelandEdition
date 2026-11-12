// Lane VKFRAME1 (2026-11-12): the bloom's level 0 held over the temporal jitter's period on Vulkan (cell_bloom_hold.frag's
// twin): the mean of the ring's newest layers. BLOOM_HOLD_RED reads the newest layer alone.
#include "BloomBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !BloomTexel( id, size ) )
		return;
#ifdef BLOOM_HOLD_RED
	const int n = 1;
#else
	const int n = BloomJobPc.ints.z;
#endif
	BloomTarget[id.xy] = float4( F4FX_BloomHoldMean( BloomRing, int2( id.xy ), BloomJobPc.ints.y, n ), 1.0 );
}
