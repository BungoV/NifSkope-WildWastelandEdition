// DDGI volume, relocation (mode 4) and classification (mode 5) (lane VKCOMPUTE1, 2026-11-12; from
// src/features/gi/rtddgi/reloc.comp). Shared, no bindings. One invocation a probe, from its 32 fixed rays
// (rtddgimath.h relocateStep / classify). job.y = 1: a scroll's pass, fresh probes only. Dispatch: ceil(probes / 64) of 64.
// RELOC_RED (the gate's red) drops the classification's write.
#ifndef DDGI_RELOC_HLSLI
#define DDGI_RELOC_HLSLI

#include "DDGIResources.hlsli"

void DDGI_RelocCS( uint p )
{
	if ( p >= uint( DDGI_P.dims.w ) )
		return;
	if ( DDGI_JOB.job.y == 1u && DDGI_ProbeState( p ) != DDGI_FRESH )
		return;   // a scroll's pass touches its fresh probes only (the kept ones keep offset and state)
	const int N = DDGI_P.counts.x;
	const float3 stepv = DDGI_P.stepv.xyz;
	if ( DDGI_JOB.job.x == 4u ) {
		// MIRROR rtddgimath.h relocateStep
		int back = 0, iBack = -1, iNear = -1, iFar = -1;
		float dBack = 1e30, dNear = 1e30, dFar = -1.0;
		for ( int i = 0; i < DDGI_FIXED; i++ ) {
			const float d = DDGIRays[p * uint( N ) + uint( i )].w;
			if ( d < 0.0 ) {
				back++;
				const float tr = -d / 0.2;
				if ( tr < dBack ) { dBack = tr; iBack = i; }
				continue;
			}
			if ( d < dNear ) { dNear = d; iNear = i; }
			if ( d > dFar ) { dFar = d; iFar = i; }
		}
		const float3 offset = DDGIProbes[p].xyz;
		const float3 lim = stepv * 0.45;
		const float mn = min( stepv.x, min( stepv.y, stepv.z ) );
		float3 o = offset;
		if ( float( back ) / float( DDGI_FIXED ) > 0.25 && iBack >= 0 ) {
			const float3 v = DDGI_Fibonacci( iBack, DDGI_FIXED ) * dBack;
			float s = 1e30;
			for ( int k = 0; k < 3; k++ ) {
				if ( abs( v[k] ) < 1e-9 )
					continue;
				const float a = ( lim[k] - offset[k] ) / v[k], b = ( -lim[k] - offset[k] ) / v[k];
				s = min( s, max( a, b ) );
			}
			s -= 1e-3;
			if ( s > 1.0 && s < 1e29 )
				o = offset + v * s;
		} else if ( iNear >= 0 && iFar >= 0 && dNear < 0.25 * mn
			&& dot( DDGI_Fibonacci( iFar, DDGI_FIXED ), DDGI_Fibonacci( iNear, DDGI_FIXED ) ) <= 0.5 ) {
			o = clamp( offset + DDGI_Fibonacci( iFar, DDGI_FIXED ) * min( 0.2 * mn, dFar ), -lim, lim );
		}
		DDGIProbes[p].xyz = o;
		return;
	}
	// MIRROR rtddgimath.h classify
	int back = 0;
	for ( int i = 0; i < DDGI_FIXED; i++ )
		back += DDGIRays[p * uint( N ) + uint( i )].w < 0.0 ? 1 : 0;
	int state = DDGI_ASLEEP;
	if ( float( back ) / float( DDGI_FIXED ) > 0.25 ) {
		state = DDGI_OFF;
	} else {
		for ( int i = 0; i < DDGI_FIXED && state == DDGI_ASLEEP; i++ ) {
			const float d = abs( DDGIRays[p * uint( N ) + uint( i )].w );
			if ( d >= DDGI_MISS * 0.5 )
				continue;
			const float3 dir = DDGI_Fibonacci( i, DDGI_FIXED );
			float leave = 1e30;
			for ( int k = 0; k < 3; k++ )
				if ( abs( dir[k] ) > 1e-6 )
					leave = min( leave, stepv[k] / abs( dir[k] ) );
			if ( d < leave )
				state = DDGI_VIGILANT;
		}
	}
#ifndef RELOC_RED
	DDGIProbes[p].w = float( state );
#endif
}

#endif
