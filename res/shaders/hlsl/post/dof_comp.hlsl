// Lane VKFRAME1 P7 (2026-11-12): the depth of field's composite on Vulkan, game_dof.frag's twin (DofCommon.hlsli).
// The game's: T0 the frame (linear), T1 its quarter blur (linear), T2 the depth (point); c[0..3] = c0..c3,
// i = (far only, the app's red). DOF_LENS (game_doflens.frag's twin): T0 the frame, T1 the far layer, T2 the depth,
// T3 the near layer; c[0] = lz, c[1] = lensP, c[2] = lensQ. PFX_RED: the game's factor ignores the depth, the lens
// drops its near layer.
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
#ifdef DOF_LENS
	float4 q = j.c[2];
#ifdef PFX_RED
	q.y = 0.0f;
#endif
	const float s1 = F4FX_LensFocus( PfxT2, j.c[0], j.c[1] );
	PFX_STORE0( id.xy, F4FX_LensTexel( int2( id.xy ), PfxT0, PfxT1, PfxT2, PfxT3, PfxLinear, s1, j.c[0], j.c[1], q ) );
#else
	uint fw, fh;
	PfxT0.GetDimensions( fw, fh );
	const float2 v1 = ( float2( id.xy ) + 0.5f ) / float2( fw, fh );
	bool red = j.i.y != 0;
#ifdef PFX_RED
	red = true;
#endif
	const float k = F4FX_DofFactor( PfxT2.SampleLevel( PfxPoint, v1, 0.0f ).r, j.c[0], j.c[1], j.c[2], j.i.x != 0, red );
	const float4 b = PfxT1.SampleLevel( PfxLinear, v1 * j.c[3].zw, 0.0f );
	const float4 a = PfxT0.SampleLevel( PfxLinear, v1, 0.0f );
	PFX_STORE0( id.xy, F4FX_DofBlend( k, a, b ) );
#endif
}
