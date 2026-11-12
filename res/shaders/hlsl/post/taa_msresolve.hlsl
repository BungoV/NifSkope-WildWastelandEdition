// Lane VKFRAME1 round 8 (2026-11-12): the linear frame's multisamples into one value for the Modern temporal AA before the
// tone map, taa2_msresolve.frag's twin (each sample weighted c / (1 + max c) at about display level, the mean turned back to
// light). T0 the multisampled frame; c[0].x = msExpose, i.x = msSamples. PFX_RED averages in light (TAA3's blit).
#define PFX_T0_MS
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	const float e = j.c[0].x;
	const int n = j.i.x;
	float3 sum = 0.0f;
	float a = 0.0f;
	for ( int i = 0; i < n; i++ ) {
		const float4 c = PfxT0.Load( int2( id.xy ), i );
		const float3 t = max( c.rgb, 0.0f ) * e;
#ifdef PFX_RED
		sum += t;
#else
		sum += t / ( 1.0f + max( t.r, max( t.g, t.b ) ) );
#endif
		a += c.a;
	}
	const float inv = 1.0f / float( max( n, 1 ) );
	const float3 t = max( min( sum * inv, 0.999f ), 0.0f );	// GL's clamp: an inf sample's NaN mean reads 0.999 (FClamp reads 0)
	PFX_STORE0( id.xy, float4( t / ( ( 1.0f - max( t.r, max( t.g, t.b ) ) ) * max( e, 1e-6f ) ), a * inv ) );
}
