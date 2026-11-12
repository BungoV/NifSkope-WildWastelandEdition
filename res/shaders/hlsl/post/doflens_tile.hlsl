// Lane VKFRAME1 P7 (2026-11-12): the thin lens's near reach on Vulkan, game_doflens_tile.frag's twin. i.x = 0: T0 the prep,
// out = the largest near radius (full px) in the 8 x 8 quarter texels of the tile; i.x = 1: T0 that, out = the largest of
// the 3 x 3 tiles around. Target R16F. PFX_RED takes the far radius in the first pass.
#define PFX_OUT0_FORMAT "r16f"
#define PFX_OUT0_TYPE float
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const int2 q = int2( id.xy );
	uint sw, sh;
	PfxT0.GetDimensions( sw, sh );
	const int2 sz = int2( sw, sh );
	float m = 0.0f;
	if ( PfxJobPc.i.x == 0 ) {
		for ( int y = 0; y < 8; y++ ) {
			for ( int x = 0; x < 8; x++ ) {
				const int2 p = min( q * 8 + int2( x, y ), sz - 1 );
#ifdef PFX_RED
				m = max( m, PfxT0.Load( int3( p, 0 ) ).a * 0.5f );
#else
				m = max( m, -PfxT0.Load( int3( p, 0 ) ).a * 0.5f );
#endif
			}
		}
	} else {
		for ( int y = -1; y <= 1; y++ ) {
			for ( int x = -1; x <= 1; x++ )
				m = max( m, PfxT0.Load( int3( clamp( q + int2( x, y ), int2( 0, 0 ), sz - 1 ), 0 ) ).r );
		}
	}
	PFX_STORE0( id.xy, m );
}
