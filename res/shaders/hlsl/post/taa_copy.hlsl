// Lane VKFRAME1 round 8 (2026-11-12): the temporal AA's picture back into the frame, game_taa_copy.frag's twin, texel for
// texel. T0 the picture. PFX_OUT0_FORMAT names the frame's format (rgba8 for the tone-mapped frame). PFX_RED reads the
// texel to the right.
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
#ifdef PFX_RED
	PFX_STORE0( id.xy, PfxT0.Load( int3( min( id.x + 1, size.x - 1 ), id.y, 0 ) ) );
#else
	PFX_STORE0( id.xy, PfxT0.Load( int3( id.xy, 0 ) ) );
#endif
}
