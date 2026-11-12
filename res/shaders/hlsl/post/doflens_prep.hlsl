// Lane VKFRAME1 P7 (2026-11-12): the thin lens's first pass on Vulkan, game_doflens_prep.frag's twin (DofCommon.hlsli).
// Quarter size: the mean of the 4x4 full-size texels (four linear taps; Inf / NaN count as 0) and the signed circle of the
// NEAREST of four depths. T0 the frame (linear), T1 the depth; c[0] = lz, c[1] = lensP, c[2] = lensQ.
// PFX_RED takes the farthest depth instead.
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	const int2 q = int2( id.xy );
	uint fw, fh, dw, dh;
	PfxT0.GetDimensions( fw, fh );
	PfxT1.GetDimensions( dw, dh );
	const float2 fs = float2( fw, fh );
	const int2 hi = int2( dw, dh ) - 1;
	const float s1 = F4FX_LensFocus( PfxT1, j.c[0], j.c[1] );
	float3 acc = float3( 0.0f, 0.0f, 0.0f );
#ifdef PFX_RED
	float zn = -1e30f;
#else
	float zn = 1e30f;
#endif
	bool sky = true;
	for ( int i = 0; i < 4; i++ ) {
		const int2 o = int2( ( i & 1 ) * 2 + 1, ( i >> 1 ) * 2 + 1 );
		const float2 pc = float2( q * 4 + o );
		acc += F4FX_ZeroNonFinite( PfxT0.SampleLevel( PfxLinear, pc / fs, 0.0f ).rgb ) * 0.25f;
		const float z = F4FX_LensDist( PfxT1.Load( int3( min( q * 4 + o, hi ), 0 ) ).r, j.c[0] );
		if ( z >= 0.0f ) {
			sky = false;
#ifdef PFX_RED
			zn = max( zn, z );
#else
			zn = min( zn, z );
#endif
		}
	}
	PFX_STORE0( id.xy, float4( acc, F4FX_LensCoc( sky ? -1.0f : zn, s1, j.c[0], j.c[1], j.c[2] ) ) );
}
