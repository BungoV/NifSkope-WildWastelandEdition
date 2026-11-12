// Lane VKFRAME1 P7 (2026-11-12): the depth of field's downsample on Vulkan, game_dof_down.frag's twin (DofCommon.hlsli).
// T0 the frame (linear); target the quarter-size blur. c[0] = c4, c[1] = c7 (alpha = c7.z), c[2..5] = the four taps.
// PFX_RED drops the last tap (the gate's red must FAIL).
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	float4 taps[4] = { j.c[2], j.c[3], j.c[4], j.c[5] };
#ifdef PFX_RED
	taps[3].z = 0.0f;
#endif
	const float2 v1 = ( float2( id.xy ) + 0.5f ) / float2( size );
	PfxOut0[id.xy] = float4( F4FX_DofDown( PfxT0, PfxLinear, v1, j.c[0], j.c[1], taps ), j.c[1].z );
}
