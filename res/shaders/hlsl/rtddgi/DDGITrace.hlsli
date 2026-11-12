// DDGI volume, the probe trace (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/rtddgi/trace.comp). Shared, no
// bindings (the shim declares the volume's buffers and DDGITLAS; DDGI_USE_TLAS, DXIL cs_6_5). One invocation a
// (probe, ray): the nearest hit on the TRUE triangles, then
//   miss          sky radiance above the horizon, the ground's below it (DDGI_Miss; 0 without sky), distance DDGI_MISS
//   backface      radiance 0, distance -0.2 t (a one-sided face seen from behind) [J21 4.1]
//   fixed ray     distance only (rays < 32 are never blended)
//   front face    albedo / pi x (sun behind a shadow ray + interior directional + the placed lights behind shadow rays
//                 + the volume's own irradiance at the hit, the infinite bounce [J19 5.3]) + Le
// job.x 0 = an update (active probes, every ray), 1 = the fixed rays of every probe (job.y = 1: fresh probes only).
// Dispatch: ceil(probes x job.w / 64) of 64. TRACE_RED (the gate's red) drops the backface rule.
#ifndef DDGI_TRACE_HLSLI
#define DDGI_TRACE_HLSLI

#include "DDGIRayQuery.hlsli"

// the bounce read's runtime red (the CLI's --red bouncenocover, src/rtddgi.cpp kRedBounceNoCover)
#define DDGI_RED_BOUNCENOCOVER 32u

// MIRROR src/rtddgi.cpp Twin::nearest: the nearest hit on the true triangles (probebvh::Bvh::ray's rule), then the tie
// pass: of every face hit within DDGI_TieWindow of that t, the one DDGI_TieWins keeps. bestPr = the BLAS row.
bool DDGI_Nearest( float3 o, float3 d, float tmax, out uint bestPr, out float bestT )
{
	bestPr = 0u;
	bestT = tmax;
	const uint red = uint( DDGI_P.flags.w );
	RayDesc r;
	r.Origin = o;
	r.TMin = 0.0;
	r.Direction = d;
	r.TMax = tmax;
	if ( ( red & DDGI_RED_HWHIT ) != 0u ) {   // the refuter: M1's rule (the hardware's hit, its barycentrics)
		RayQuery<RAY_FLAG_NONE> q;
		q.TraceRayInline( DDGITLAS, RAY_FLAG_NONE, 0xFF, r );
		while ( q.Proceed() ) {
			if ( q.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE )
				continue;
			const uint pr = DDGI_CandidatePrim( q );
			const float2 bc = q.CandidateTriangleBarycentrics();
			if ( !DDGI_Hole( DDGIRemap[pr], bc.x, bc.y ) )
				q.CommitNonOpaqueTriangleHit();
		}
		if ( q.CommittedStatus() != COMMITTED_TRIANGLE_HIT )
			return false;
		bestT = q.CommittedRayT();
		bestPr = DDGI_Prim( q.CommittedInstanceID(), q.CommittedGeometryIndex(), q.CommittedPrimitiveIndex() );
		return true;
	}
	bool hit = false;
	{
		RayQuery<RAY_FLAG_NONE> q;
		q.TraceRayInline( DDGITLAS, RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, r );
		while ( q.Proceed() ) {
			if ( q.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE )
				continue;
			const uint pr = DDGI_CandidatePrim( q );
			float t, u, v;
			if ( DDGI_TriTest( pr, o, d, t, u, v ) && t > DDGI_TMIN && t <= bestT && !DDGI_Hole( DDGIRemap[pr], u, v ) ) {
				bestT = t;
				bestPr = pr;
				hit = true;
				q.CommitNonOpaqueTriangleHit();   // the hardware culls past it; our own bestT decides
			}
		}
	}
	if ( !hit || ( red & DDGI_RED_NOTIE ) != 0u )
		return hit;
	// the tie pass: a short query over [t0 - w, t0 + w], nothing committed, every face in it seen
	const float t0 = bestT;
	const float w = DDGI_TieWindow( o, t0 );
	int keepTri = DDGIRemap[bestPr];
	bool keepBack = DDGI_BackFace( bestPr, d );
	RayDesc r2;
	r2.Origin = o;
	r2.TMin = max( t0 - w, 0.0 );
	r2.Direction = d;
	r2.TMax = t0 + w;
	RayQuery<RAY_FLAG_NONE> q2;
	q2.TraceRayInline( DDGITLAS, RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, r2 );
	while ( q2.Proceed() ) {
		if ( q2.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE )
			continue;
		const uint pr = DDGI_CandidatePrim( q2 );
		float t, u, v;
		if ( DDGI_TriTest( pr, o, d, t, u, v ) && t > DDGI_TMIN && t >= t0 - w && t <= t0 + w && !DDGI_Hole( DDGIRemap[pr], u, v ) ) {
			const bool back = DDGI_BackFace( pr, d );
			const int tri = DDGIRemap[pr];
			if ( DDGI_TieWins( back, tri, keepBack, keepTri ) ) {
				keepTri = tri;
				keepBack = back;
				bestPr = pr;
				bestT = t;
			}
		}
	}
	return true;
}

// MIRROR rtddgimath.h flatAmbient: the flat ambient's irradiance at n from the sky cube (0 without a sky)
float3 DDGI_FlatAmbient( float3 n )
{
	if ( DDGI_P.sky[0].w <= 0.5 )
		return float3( 0.0, 0.0, 0.0 );
	const float3 s = DDGI_P.sky[0].rgb + DDGI_P.sky[1].rgb + DDGI_P.sky[2].rgb + DDGI_P.sky[3].rgb + DDGI_P.sky[4].rgb + DDGI_P.sky[5].rgb;
	const float3 ex = 0.5 * DDGI_PI * ( n.x >= 0.0 ? DDGI_P.sky[0].rgb : DDGI_P.sky[1].rgb ) + 0.125 * DDGI_PI * ( s - DDGI_P.sky[0].rgb - DDGI_P.sky[1].rgb );
	const float3 ey = 0.5 * DDGI_PI * ( n.y >= 0.0 ? DDGI_P.sky[2].rgb : DDGI_P.sky[3].rgb ) + 0.125 * DDGI_PI * ( s - DDGI_P.sky[2].rgb - DDGI_P.sky[3].rgb );
	const float3 ez = 0.5 * DDGI_PI * ( n.z >= 0.0 ? DDGI_P.sky[4].rgb : DDGI_P.sky[5].rgb ) + 0.125 * DDGI_PI * ( s - DDGI_P.sky[4].rgb - DDGI_P.sky[5].rgb );
	return n.x * n.x * ex + n.y * n.y * ey + n.z * n.z * ez;
}

// MIRROR src/rtddgi.cpp sampleVolume: the 8-probe irradiance at Pw (normal n, viewer direction o), x 2 pi, x the fade
// (box faces x cover: rtddgimath.h sampleVolume), the flat ambient for the rest (bounceIrradiance)
float3 DDGI_IrradianceAt( float3 Pw, float3 n, float3 o )
{
	const float D = DDGI_P.stepv.w;
	float3 Pb = Pw;
	if ( ( uint( DDGI_P.flags.w ) & DDGI_RED_NOBIAS ) == 0u )
		Pb += DDGI_BiasVector( n, o, D, DDGI_P.blendp.z );
	const float3 g = ( Pb - DDGI_P.origin.xyz ) / DDGI_P.stepv.xyz;
	const int3 base = clamp( int3( floor( g ) ), int3( 0, 0, 0 ), max( DDGI_P.dims.xyz - int3( 2, 2, 2 ), int3( 0, 0, 0 ) ) );
	const float3 a = clamp( g - float3( base ), float3( 0.0, 0.0, 0.0 ), float3( 1.0, 1.0, 1.0 ) );
	const float3 e3 = clamp( min( g, float3( DDGI_P.dims.xyz - int3( 1, 1, 1 ) ) - g ), float3( 0.0, 0.0, 0.0 ), float3( 1.0, 1.0, 1.0 ) );   // MIRROR cascadeEdge (untight)
	const float fade = min( e3.x, min( e3.y, e3.z ) );
	float3 sum = float3( 0.0, 0.0, 0.0 );
	float wsum = 0.0;
	float cover = 0.0;
	for ( int c = 0; c < 8; c++ ) {
		const int3 off = int3( c & 1, ( c >> 1 ) & 1, ( c >> 2 ) & 1 );
		const int3 pc3 = min( base + off, DDGI_P.dims.xyz - int3( 1, 1, 1 ) );
		const uint p = uint( pc3.x + DDGI_P.dims.x * ( pc3.y + DDGI_P.dims.y * pc3.z ) );
		const float3 tri = lerp( float3( 1.0, 1.0, 1.0 ) - a, a, float3( off ) );
		const float trilinear = tri.x * tri.y * tri.z;
		const int st = DDGI_ProbeState( p );
		if ( st != DDGI_VIGILANT || DDGIAge[p] == 0u ) {
			cover += st == DDGI_OFF ? trilinear : 0.0;   // MIRROR rtddgimath.h probeMissing: Off keeps its cover
			continue;
		}
		cover += trilinear;
		const float3 pp = DDGI_ProbePos( p );
		const float3 toProbe = normalize( pp - Pw );
		float w = DDGI_BackfaceWeight( toProbe, n );
		const float3 bToProbe = pp - Pb;
		const float r = length( bToProbe );
		if ( ( uint( DDGI_P.flags.w ) & DDGI_RED_NOCHEB ) == 0u && r > 0.0 ) {
			const float2 m = DDGI_SampleDist( p, -bToProbe / r );
			w *= DDGI_Chebyshev( r, m.x, m.y );
		}
		w = max( w, 1e-6 );
		w = DDGI_Crush( w );
		w *= trilinear;
		const float3 e = DDGI_SampleIrr( p, n );
		sum += w * pow( max( e, float3( 0.0, 0.0, 0.0 ) ), float3( 2.5, 2.5, 2.5 ) );
		wsum += w;
	}
	const bool rule = ( uint( DDGI_P.flags.w ) & DDGI_RED_BOUNCENOCOVER ) == 0u;
	if ( wsum <= 0.0 )
		return rule ? DDGI_FlatAmbient( n ) : float3( 0.0, 0.0, 0.0 );
	const float3 m = sum / wsum;
	const float3 E = m * m * ( 2.0 * DDGI_PI );
	if ( !rule )
		return E;
	const float f = clamp( fade * min( cover, 1.0 ), 0.0, 1.0 );
	return E * f + DDGI_FlatAmbient( n ) * ( 1.0 - f );
}

// the direct light at a hit: the sun behind a shadow ray, an interior's directional, the placed lights behind shadow rays
float3 DDGI_Direct( float3 Pw, float3 n )
{
	const bool hw = ( uint( DDGI_P.flags.w ) & DDGI_RED_HWHIT ) != 0u;
	float3 E = float3( 0.0, 0.0, 0.0 );
	const float3 o = Pw + n * 0.5;
	if ( DDGI_P.sunTo.w > 0.5 ) {
		const float nl = dot( n, DDGI_P.sunTo.xyz );
		if ( nl > 0.0 && !DDGI_AnyHit( o, DDGI_P.sunTo.xyz, DDGI_P.sun.w, hw, false ) )
			E += DDGI_P.sun.rgb * nl;
	}
	if ( DDGI_P.dirTo.w > 0.5 ) {
		const float nl = dot( n, DDGI_P.dirTo.xyz );
		if ( nl > 0.0 )
			E += DDGI_P.dirColor.rgb * nl;
	}
	for ( int li = 0; li < DDGI_P.counts.y; li++ ) {
		const uint b = uint( li ) * 8u;
		const float4 l0 = DDGILights[b], l1 = DDGILights[b + 1u], l2 = DDGILights[b + 2u], l3 = DDGILights[b + 3u];
		const float4 l7 = DDGILights[b + 7u];
		const float3 Lv = l0.xyz - Pw;
		const float d = length( Lv );
		if ( d >= l0.w )
			continue;
		const int shape = int( l7.x + 0.5 );
		if ( shape == 1 && dot( Pw - l0.xyz, l2.xyz ) < 0.0 )
			continue;
		if ( shape == 2 ) {
			bool inside = true;
			for ( int k = 0; k < 3; k++ ) {
				const float4 bx = DDGILights[b + 4u + uint( k )];
				if ( abs( dot( bx.xyz, Pw ) + bx.w ) > 1.0 )
					inside = false;
			}
			if ( !inside )
				continue;
		}
		const float3 L = Lv / max( d, 0.001 );
		const float nl = dot( n, L );
		if ( nl <= 0.0 )
			continue;
		float att = DDGI_Radial( d, l0.w, l3.x, l3.y, l3.z );
		if ( l1.w > 0.5 )
			att *= DDGI_Spot( -dot( L, l2.xyz ), l2.w, l3.w );
		if ( att * nl <= 0.0 )
			continue;
		const float3 dl = l0.xyz - o;
		const float len = length( dl );
		if ( DDGI_AnyHit( o, dl / max( len, 1e-6 ), len - DDGI_P.dirColor.w, hw, false ) )
			continue;
		E += l1.rgb * att * nl;
	}
	return E;
}

void DDGI_TraceCS( uint id )
{
	const uint nRay = DDGI_JOB.job.w;
	if ( id >= uint( DDGI_P.dims.w ) * nRay )
		return;
	const uint p = id / nRay;
	const int i = int( DDGI_JOB.job.z + id % nRay );
	const int N = DDGI_P.counts.x;
	const int st = DDGI_ProbeState( p );
	if ( DDGI_JOB.job.x == 0u && ( st == DDGI_OFF || ( st == DDGI_ASLEEP && DDGI_P.flags.y != 0 ) || DDGI_Skipped( DDGI_P, p ) ) )
		return;   // and a coarse probe a finer cascade covers (DDGI_Skipped: off unless the cell view sets it)
	if ( DDGI_JOB.job.x == 1u && DDGI_JOB.job.y == 1u && st != DDGI_FRESH )
		return;   // a scroll relocates and classifies its fresh probes only
	const float3 o = DDGI_ProbePos( p );
	const float3 d = DDGI_JobRayDir( i, N );
	uint pr;
	float t;
	float4 rec;
	if ( !DDGI_Nearest( o, d, DDGI_P.sun.w, pr, t ) ) {
		rec = float4( DDGI_P.sky[0].w > 0.5 && i >= DDGI_FIXED ? DDGI_Miss( DDGI_P, d ) : float3( 0.0, 0.0, 0.0 ), DDGI_MISS );   // ground below
	} else {
		const int tri = DDGIRemap[pr];
		const uint vb = pr * 9u;
		const float3 v0 = float3( DDGIVerts[vb], DDGIVerts[vb + 1u], DDGIVerts[vb + 2u] );
		const float3 e1 = float3( DDGIVerts[vb + 3u], DDGIVerts[vb + 4u], DDGIVerts[vb + 5u] ) - v0;
		const float3 e2 = float3( DDGIVerts[vb + 6u], DDGIVerts[vb + 7u], DDGIVerts[vb + 8u] ) - v0;
		const float3 nf = normalize( cross( e1, e2 ) );
		const float4 a0 = DDGITriAttr[uint( tri ) * 2u];
		const bool back = a0.w < 0.5 && dot( nf, d ) > 0.0;
#ifndef TRACE_RED
		const bool backRule = back && ( uint( DDGI_P.flags.w ) & DDGI_RED_NOBACK ) == 0u;
#else
		const bool backRule = false;
#endif
		if ( backRule ) {
			rec = float4( 0.0, 0.0, 0.0, -0.2 * t );
		} else if ( i < DDGI_FIXED ) {
			rec = float4( 0.0, 0.0, 0.0, t );
		} else {
			const float3 n = dot( nf, d ) > 0.0 ? -nf : nf;
			const float3 Pw = o + d * t;
			float3 E = DDGI_Direct( Pw, n );
			if ( DDGI_P.flags.x == 0 )
				E += DDGI_IrradianceAt( Pw, n, -d );
			rec = float4( a0.rgb * E / DDGI_PI + DDGITriAttr[uint( tri ) * 2u + 1u].rgb, t );
		}
	}
	DDGIRays[p * uint( N ) + uint( i )] = rec;
}

#endif
