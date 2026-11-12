// Lane VKFRAME1 P7 (2026-11-12): per-object motion blur passes 2 and 3 on Vulkan, cell_objblur_tile.frag's twin. T0 the pixel
// vectors (i.y = 0: the longest of the tile x tile pixels, i.x = tile) or the tile max (i.y = 1: the longest of the 3 x 3
// tiles around). Target RG16F. PFX_RED keeps the tile's first pixel instead of its longest.
#define PFX_OUT0_FORMAT "rg16f"
#define PFX_OUT0_TYPE float2
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const int tile = PfxJobPc.i.x;
	const int2 t = int2( id.xy );
	uint sw, sh;
	PfxT0.GetDimensions( sw, sh );
	const int2 sz = int2( sw, sh );
	float2 best = float2( 0.0f, 0.0f );
	float bl = 0.0f;
	if ( PfxJobPc.i.y == 0 ) {
		const int2 o = t * tile;
#ifdef PFX_RED
		best = PfxT0.Load( int3( o, 0 ) ).xy;
#else
		for ( int y = 0; y < tile; y++ )
			for ( int x = 0; x < tile; x++ ) {
				const int2 q = o + int2( x, y );
				if ( q.x >= sz.x || q.y >= sz.y )
					continue;
				F4FX_ObjBlurLonger( PfxT0.Load( int3( q, 0 ) ).xy, best, bl );
			}
#endif
	} else {
		for ( int y = -1; y <= 1; y++ )
			for ( int x = -1; x <= 1; x++ )
				F4FX_ObjBlurLonger( PfxT0.Load( int3( clamp( t + int2( x, y ), int2( 0, 0 ), sz - 1 ), 0 ) ).xy, best, bl );
	}
	PfxOut0[id.xy] = best;
}
