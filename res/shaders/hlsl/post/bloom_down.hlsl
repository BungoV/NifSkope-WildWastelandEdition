// Lane VKFRAME1 (2026-11-12): one down level of the bloom pyramid on Vulkan (cell_bloom_down.frag's twin; the math is
// BloomCommon.hlsli, the Fusion Shaders BloomDownCS original). Gate: tests/vkframe (the GL pass and this kernel on the same
// input, compared on the GPU); BLOOM_DOWN_RED drops the Karis average.
#include "BloomBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !BloomTexel( id, size ) )
		return;
	uint sw, sh;
	BloomSource.GetDimensions( sw, sh );
	const float2 texel = 1.0 / float2( max( uint2( sw, sh ), uint2( 1, 1 ) ) );
	const float2 uv = ( float2( id.xy ) + 0.5 ) / float2( size );
	const float4 p = BloomJobPc.params;
	const float prefilter = clamp( p.z, 0.0, 1.0 );
#ifdef BLOOM_DOWN_RED
	const float karis = 0.0;
#else
	const float karis = BloomJobPc.ints.x == 1 ? 0.0 : prefilter;
#endif
	BloomTarget[id.xy] = float4( F4FX_BloomDown( BloomSource, BloomLinearClamp, uv, texel, p.x, p.y, prefilter, karis, p.w ), 1.0 );
}
