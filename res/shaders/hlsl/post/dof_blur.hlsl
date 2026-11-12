// Lane VKFRAME1 P7 (2026-11-12): one axis of the depth of field's blur on Vulkan, game_dof_blur.frag's twin (the game's
// Blur3..Blur15). T0 the source (point); c[0..3] = the taps' offsets along the axis (16 floats, the app's divisions);
// i = (taps, weight row, first weight k, axis: 0 the columns (y), 1 the rows (x)). PFX_RED reads the row below's weights.
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	const float2 v1 = ( float2( id.xy ) + 0.5f ) / float2( size );
	int row = j.i.y;
#ifdef PFX_RED
	row = max( row - 1, 0 );
#endif
	float4 acc = float4( 0.0f, 0.0f, 0.0f, 0.0f );
	for ( int t = 0; t < j.i.x; t++ ) {
		const float o = j.c[t >> 2][t & 3];
		const float2 off = j.i.w == 1 ? float2( o, 0.0f ) : float2( 0.0f, o );
		acc = PfxT0.SampleLevel( PfxPoint, v1 + off, 0.0f ) * F4FX_BlurWeights[row][j.i.z + t] + acc;
	}
	PfxOut0[id.xy] = acc;
}
