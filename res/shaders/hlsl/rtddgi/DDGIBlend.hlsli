// DDGI volume, the probe blend (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/rtddgi/blend.comp). Shared, no bindings.
// blend.comp's header:
// Lane RTXGI1: the probe blend. One workgroup a probe: its rays into shared memory, then threads 0..35 update the
// 6x6 irradiance interior and threads 36..231 the 14x14 distance interior (src/rtddgimath.h blendIrradianceTexel /
// blendDistanceTexel, the CPU twin's arithmetic in float). The fixed rays (< 32) are not blended [SDK]. M1FIX: the
// irradiance texel's alpha holds its sky visibility (blendSkyVisTexel).
// HLSL: one group of 256 a probe, the rays in group-shared memory. Dispatch: one group a probe. BLEND_RED (the gate's red) drops the distance write.
#ifndef DDGI_BLEND_HLSLI
#define DDGI_BLEND_HLSLI

#include "DDGIResources.hlsli"

groupshared float4 DDGIBlendRay[256];
groupshared float3 DDGIBlendDir[256];

void DDGI_BlendCS( uint p, uint t )
{
	if ( p >= uint( DDGI_P.dims.w ) )
		return;
	const int st = DDGI_ProbeState( p );
	if ( st == DDGI_OFF || ( st == DDGI_ASLEEP && DDGI_P.flags.y != 0 ) || DDGI_Skipped( DDGI_P, p ) )
		return;   // the whole group leaves together: no barrier is skipped by part of it (RTXCASCADE2: + a covered probe)
	const int N = DDGI_P.counts.x;
	const int nb = N - DDGI_FIXED;
	if ( int( t ) < nb ) {
		DDGIBlendRay[t] = DDGIRays[p * uint( N ) + uint( DDGI_FIXED ) + t];
		DDGIBlendDir[t] = DDGI_JobRayDir( DDGI_FIXED + int( t ), N );
	}
	GroupMemoryBarrierWithGroupSync();
	const float hBase = DDGI_AgeHysteresis( DDGIAge[p], DDGI_P.blendp.x );
	if ( t < 36u ) {
		// MIRROR rtddgimath.h blendIrradianceTexel
		const int tx = int( t ) % DDGI_IRR_N, ty = int( t ) / DDGI_IRR_N;
		// + MIRROR rtddgimath.h blendSkyVisTexel (the alpha): the cosine-weighted miss share, no gamma, this texel's h
		const float3 td = DDGI_TexelDir( tx, ty, DDGI_IRR_N );
		float3 s = float3( 0.0, 0.0, 0.0 );
		float w = 0.0, sv = 0.0;
		for ( int r = 0; r < nb; r++ ) {
			const float c = max( 0.0, dot( td, DDGIBlendDir[r] ) );
			if ( c <= 0.0 )
				continue;
			s += c * DDGIBlendRay[r].rgb;
			sv += DDGI_SkyVisRay( DDGI_P, DDGIBlendDir[r], DDGIBlendRay[r].w ) ? c : 0.0;   // SKYBELOW1: a miss above the horizon only
			w += c;
		}
		const uint at = p * uint( DDGI_IRR_S * DDGI_IRR_S ) + uint( ( ty + 1 ) * DDGI_IRR_S + tx + 1 );
		if ( w > 0.0 ) {
			const float4 old = DDGIIrradiance[at];
			const float3 m = 0.5 * s / w;
			const float3 enc = float3( DDGI_GammaEncode( m.x ), DDGI_GammaEncode( m.y ), DDGI_GammaEncode( m.z ) );
			const float h = DDGI_TexelHysteresis( hBase, old.rgb, enc );
			DDGIIrradiance[at] = float4( enc * ( 1.0 - h ) + old.rgb * h, ( sv / w ) * ( 1.0 - h ) + old.a * h );
		}
	} else if ( t < 36u + 196u ) {
		// MIRROR rtddgimath.h blendDistanceTexel
		const int k = int( t ) - 36;
		const int tx = k % DDGI_DIST_N, ty = k / DDGI_DIST_N;
		const float3 td = DDGI_TexelDir( tx, ty, DDGI_DIST_N );
		float s = 0.0, s2 = 0.0, w = 0.0;
		for ( int r = 0; r < nb; r++ ) {
			const float c = max( 0.0, dot( td, DDGIBlendDir[r] ) );
			const float wr = pow( c, DDGI_P.blendp.y );
			if ( wr < 0.001 )
				continue;
			const float d = min( abs( DDGIBlendRay[r].w ), DDGI_P.origin.w );
			s += wr * d;
			s2 += wr * d * d;
			w += wr;
		}
		const uint at = p * uint( DDGI_DIST_S * DDGI_DIST_S ) + uint( ( ty + 1 ) * DDGI_DIST_S + tx + 1 );
#ifndef BLEND_RED
		if ( w > 0.0 ) {
			const float2 old = DDGIDistance[at];
			DDGIDistance[at] = float2( s / w, s2 / w ) * ( 1.0 - hBase ) + old * hBase;
		}
#endif
	}
}

#endif
