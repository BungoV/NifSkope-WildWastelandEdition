// Lane VKFRAME1 (2026-11-12): one up level of the bloom pyramid on Vulkan (cell_bloom_up.frag's twin; the math is
// BloomCommon.hlsli, the Fusion Shaders BloomUpCS original): band x source weight + tent(accumulated) x bloom weight.
// BLOOM_UP_RED drops the dispersion.
#include "BloomBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !BloomTexel( id, size ) )
		return;
	uint mw, mh;
	BloomMip.GetDimensions( mw, mh );
	const float2 texel = 1.0 / float2( max( uint2( mw, mh ), uint2( 1, 1 ) ) );
	const float2 uv = ( float2( id.xy ) + 0.5 ) / float2( size );
	const float4 p = BloomJobPc.params;
#ifdef BLOOM_UP_RED
	const float dispersion = 0.0;
#else
	const float dispersion = p.w;
#endif
	BloomTarget[id.xy] = float4( F4FX_BloomUp( BloomSource, BloomMip, BloomLinearClamp, uv, texel, p.x, p.y, p.z, dispersion ), 1.0 );
}
