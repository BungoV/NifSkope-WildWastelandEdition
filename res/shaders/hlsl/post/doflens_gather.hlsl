// Lane VKFRAME1 P7 (2026-11-12): the thin lens's gather on Vulkan, game_doflens_gather.frag's twin. Quarter size, 32 taps on
// a Vogel disk; out 0 the far layer, out 1 the near layer (premultiplied). T0 the prep (linear), T1 the near reach (the tile
// pass); c[2] = lensQ. PFX_RED spaces the taps linearly in radius (no sqrt).
#define PFX_OUT1 1
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const float4 lensQ = PfxJobPc.c[2];
	const int N = 32;
	const int2 q = int2( id.xy );
	uint qw, qh;
	PfxT0.GetDimensions( qw, qh );
	const float2 qs = float2( qw, qh );
	const float4 c0 = PfxT0.Load( int3( q, 0 ) );
	const float cc = c0.a;
	const float rc = max( cc, 0.0f ) * 0.5f;
	const float rn = lensQ.y == 0.0f ? 0.0f : lensQ.w > 0.5f ? PfxT1.Load( int3( q / 8, 0 ) ).r : lensQ.x * 0.5f;
	const float rk = max( rc, rn );
	if ( rk < 0.5f ) {
		PFX_STORE0( id.xy, float4( c0.rgb, 1.0f ) );
		PFX_STORE1( id.xy, float4( 0.0f, 0.0f, 0.0f, 0.0f ) );
		return;
	}
	const float2 uc = ( float2( q ) + 0.5f ) / qs;
	float3 fa = float3( 0.0f, 0.0f, 0.0f ), na = float3( 0.0f, 0.0f, 0.0f );
	float fw = 0.0f, nd = 0.0f;
	const float share = rk * rk / float( N );
	for ( int i = 0; i < N; i++ ) {
#ifdef PFX_RED
		const float t = ( ( float( i ) + 0.5f ) / float( N ) ) * rk;
#else
		const float t = sqrt( ( float( i ) + 0.5f ) / float( N ) ) * rk;
#endif
		const float an = float( i ) * 2.39996323f;
		const float2 o = float2( cos( an ), sin( an ) ) * t;
		const float4 cs = PfxT0.SampleLevel( PfxLinear, uc + o * 0.25f / qs, 0.0f );
		float rf = max( cs.a, 0.0f ) * 0.5f;
		if ( cs.a > cc )
			rf = min( rf, rc );
		const float w = F4FX_PostSat( rf - t + 0.5f );
		fa += cs.rgb * w;
		fw += w;
		const float rN = max( -cs.a, 0.0f ) * 0.5f;
		const float d = F4FX_PostSat( rN - t + 0.5f ) * share / max( rN * rN, 1.0f );
		na += cs.rgb * d;
		nd += d;
	}
	PFX_STORE0( id.xy, float4( fw > 0.0001f ? fa / fw : c0.rgb, 1.0f ) );
	const float a = F4FX_PostSat( nd );
	PFX_STORE1( id.xy, nd > 0.0f ? float4( na / nd * a, a ) : float4( 0.0f, 0.0f, 0.0f, 0.0f ) );
}
