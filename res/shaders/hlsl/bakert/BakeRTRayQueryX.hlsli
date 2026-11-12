// Bake tracer: the whole-ray RT tracer (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/bakert/rqx.comp, lanes RTGI1 +
// ROOMFIT10). One thread a ray answers what the CPU's probebvh walk answers (the nearest accepted triangle, or for an
// occlusion ray whether any is accepted) and says whether it is SURE. Every hardware candidate is re-tested in double
// (probebvh::Bvh::tri's arithmetic, AlphaMask::hole in double); the reason bits say why a ray is unsure:
//   1 an edge band, 2 a texel / threshold border, 4 the determinant cut, 8 the ray's start or end, 16 two hits within the
//   near pad, 32 phase 2 found no accepted hit. Phase 1 finds the nearest hit; phase 2 re-walks up to it + the float error.
// Shared, no bindings: BakeRTXRays, BakeRTHits, BakeRTTLAS, BakeRTRemap, BakeRTVerts. Hits: soup triangle, reason bits.
// Doubles: SPIR-V needs shaderFloat64 (bakert.cpp's device asks for it); DXIL needs the double ops (see BakeRTCommon).
#ifndef BAKERT_RAY_QUERY_X_HLSLI
#define BAKERT_RAY_QUERY_X_HLSLI

#include "BakeRTCommon.hlsli"

static const double kBakeRTNearPad = 1e-6L;   // bakertwalk.h
static const double kBakeRTErr = 4e-6L;       // the float trace's error, relative to the coordinates' size (~30 float ulps)
// ROOMFIT10: a grazing ray's hardware t is off by the float origin's rounding over cos: phase 2 reaches this much further
static const double kBakeRTGraze = 1e-4L;

// AlphaMask::hole in double; why |= 2 when the texel or threshold decision sat on its edge
bool BakeRT_HoleD( int tri, double b1, double b2, inout uint why )
{
	if ( BAKERT_PC.useMask == 0u || tri < 0 || uint( tri ) >= BAKERT_PC.nTri )
		return false;
	const int m = BakeRTTriOf[tri];
	if ( m < 0 )
		return false;
	const uint b = uint( m ) * 12u;
	const double b0 = 1.0L - b1 - b2;
	double u = b0 * double( BakeRT_Mf( b + 2u ) ) + b1 * double( BakeRT_Mf( b + 4u ) ) + b2 * double( BakeRT_Mf( b + 6u ) );
	double v = b0 * double( BakeRT_Mf( b + 3u ) ) + b1 * double( BakeRT_Mf( b + 5u ) ) + b2 * double( BakeRT_Mf( b + 7u ) );
	const uint4 mp = BakeRTMaps[BakeRTMaskTris[b]];
	u -= BakeRT_FloorD( u );
	v -= BakeRT_FloorD( v );
	const double xu = u * BakeRT_UToD( mp.y ), yv = v * BakeRT_UToD( mp.z );
	const double fx = xu - BakeRT_FloorD( xu ), fy = yv - BakeRT_FloorD( yv );
	if ( fx < 1e-7L || fx > 1.0L - 1e-7L || fy < 1e-7L || fy > 1.0L - 1e-7L || u < 1e-9L || v < 1e-9L )
		why |= 2u;
	const int x = clamp( int( xu ), 0, int( mp.y ) - 1 );
	const int y = clamp( int( yv ), 0, int( mp.z ) - 1 );
	const uint off = mp.x + uint( y ) * mp.y + uint( x );
	const uint a = ( BakeRTAlpha[off >> 2u] >> ( ( off & 3u ) * 8u ) ) & 255u;
	const uint thr = BakeRTMaskTris[b + 1u];
	if ( BakeRTMaskTris[b + 11u] == 0u )
		return a < thr;
	const double s = b0 * double( BakeRT_Mf( b + 8u ) ) + b1 * double( BakeRT_Mf( b + 9u ) ) + b2 * double( BakeRT_Mf( b + 10u ) );
	const double lhs = BakeRT_UToD( a ) * s;
	if ( abs( lhs - BakeRT_UToD( thr ) ) <= 1e-9L * ( 1.0L + BakeRT_UToD( thr ) ) )
		why |= 2u;
	return lhs < BakeRT_UToD( thr );
}

/* BLAS primitive p against the double ray: probebvh::Bvh::tri, then (1e-4, tmax] and the hole. Returns accepted;
 * tt = its distance; why = which of those decisions sat within its error band (the reason bits). */
bool BakeRT_TestD( uint p, double3 o, double3 d, double tmax, double eDist, out double tt, inout uint why )
{
	tt = 0.0L;
	const uint b = p * 9u;
	const double3 p0 = double3( BakeRTVerts[b], BakeRTVerts[b + 1u], BakeRTVerts[b + 2u] );
	const double3 p1 = double3( BakeRTVerts[b + 3u], BakeRTVerts[b + 4u], BakeRTVerts[b + 5u] );
	const double3 p2 = double3( BakeRTVerts[b + 6u], BakeRTVerts[b + 7u], BakeRTVerts[b + 8u] );
	const double3 e1 = p1 - p0, e2 = p2 - p0;
	const double3 pv = double3( d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z, d.x * e2.y - d.y * e2.x );
	const double det = e1.x * pv.x + e1.y * pv.y + e1.z * pv.z;
	if ( abs( abs( det ) - 1e-12L ) <= 1e-18L )
		why |= 4u;
	if ( abs( det ) < 1e-12L )
		return false;
	const double id = 1.0L / det;
	const double3 tv = o - p0;
	const double u = ( tv.x * pv.x + tv.y * pv.y + tv.z * pv.z ) * id;
	const double3 qv = double3( tv.y * e1.z - tv.z * e1.y, tv.z * e1.x - tv.x * e1.z, tv.x * e1.y - tv.y * e1.x );
	const double v = ( d.x * qv.x + d.y * qv.y + d.z * qv.z ) * id;
	tt = ( e2.x * qv.x + e2.y * qv.y + e2.z * qv.z ) * id;
	// the hit point's distance to each edge line: barycentric x that edge's height (2 area / edge length)
	const double3 nn = BakeRT_CrossD( e1, e2 );
	const double a2 = BakeRT_LengthD( nn );
	const double h0 = a2 / max( BakeRT_LengthD( p2 - p1 ), 1e-30L ), h1 = a2 / max( BakeRT_LengthD( e2 ), 1e-30L ),
		h2 = a2 / max( BakeRT_LengthD( e1 ), 1e-30L );
	const double w = 1.0L - u - v;
	/* ROOMFIT10: on a grown BLAS the edge band only has to cover the GPU's double rounding against the CPU's; kept at the
	 * float band when the growth does not cover the float error, for a grazing ray (|cos| < 4e-3) and for a sliver */
	double eEdge = eDist;
	if ( BAKERT_PC.grow > 0.0 && eDist <= 0.9L * double( BAKERT_PC.grow ) && abs( BakeRT_DotD( nn, d ) ) >= 4e-3L * a2 * BakeRT_LengthD( d ) ) {
		const double l0 = BakeRT_LengthD( p2 - p1 ), l1 = BakeRT_LengthD( e2 ), l2 = BakeRT_LengthD( e1 );
		const double c0 = BakeRT_DotD( e1, e2 ) / max( l1 * l2, 1e-300L ), c1 = BakeRT_DotD( -e1, p2 - p1 ) / max( l2 * l0, 1e-300L ),
			c2 = BakeRT_DotD( -e2, p1 - p2 ) / max( l1 * l0, 1e-300L );
		// sin(half angle)^2 = (1 - cos) / 2 >= 4e-6 at every corner
		if ( min( c0, min( c1, c2 ) ) > -2.0L && max( c0, max( c1, c2 ) ) <= 1.0L - 8e-6L )
			eEdge = eDist * 2.5e-4L;
	}
	if ( abs( w ) * h0 <= eEdge || abs( u ) * h1 <= eEdge || abs( v ) * h2 <= eEdge )
		why |= 1u;
	if ( u < 0.0L || u > 1.0L || v < 0.0L || u + v > 1.0L )
		return false;
	if ( abs( tt - 1e-4L ) <= 1e-9L || abs( tt - tmax ) <= kBakeRTNearPad * ( 1.0L + tmax ) )
		why |= 8u;
	if ( !( tt > 1e-4L ) || tt > tmax )
		return false;
	if ( BakeRT_HoleD( BakeRTRemap[p], u, v, why ) )
		return false;
	return true;
}

// the BLAS primitive (opaque ones first, then the alpha-tested) of a query's candidate or commit
#define BAKERT_PRIM_CAND( Q ) ( Q.CandidatePrimitiveIndex() + ( Q.CandidateGeometryIndex() == 1u ? BAKERT_PC.nOpaque : 0u ) )
#define BAKERT_PRIM_COMMIT( Q ) ( Q.CommittedPrimitiveIndex() + ( Q.CommittedGeometryIndex() == 1u ? BAKERT_PC.nOpaque : 0u ) )

void BakeRT_RayQueryXCS( uint k )
{
	if ( k >= BAKERT_PC.count )
		return;
	const BakeRTXRay r = BakeRTXRays[BAKERT_PC.first + k];
	const double3 o = r.o, d = r.d;
	const double tmax = r.tmax;
	const bool any = ( BAKERT_PC.flags & 1u ) != 0u;
	const double oMag = max( abs( o.x ), max( abs( o.y ), abs( o.z ) ) );
	// ROOMFIT10: the float ray starts `back` behind the true origin (a point on a surface meets a triangle inside the float
	// origin's rounding); the double test uses the true origin
	const double back = kBakeRTErr * ( oMag + 1.0L );
	const float3 of = float3( o - d * back ), df = float3( d );
	const float tmaxF = float( tmax * ( 1.0L + kBakeRTErr ) + kBakeRTErr * ( oMag + 1.0L ) + back );
	uint why = 0u;   // the unsure reasons (0 = sure)
	int tri = -1;

	// phase 1: the nearest hit; every triangle goes through the double test (no opaque commits in hardware)
	RayDesc rd;
	rd.Origin = of;
	rd.TMin = 0.0;
	rd.Direction = df;
	rd.TMax = tmaxF;
	RayQuery<RAY_FLAG_NONE> q;
	q.TraceRayInline( BakeRTTLAS, any ? RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_NON_OPAQUE : RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, rd );
	while ( q.Proceed() ) {
		if ( q.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE )
			continue;
		const uint p = BAKERT_PRIM_CAND( q );
		const float tf = q.CandidateTriangleRayT();
		double tt;
		uint m = 0u;
		const bool acc = BakeRT_TestD( p, o, d, tmax, kBakeRTErr * ( oMag + double( tf ) + 1.0L ), tt, m );
		why |= m;
		if ( any ) {   // an occlusion ray ends on its first SURE accepted hit; a doubtful one only marks the ray
			if ( acc && m == 0u ) {
				tri = BakeRTRemap[p];
				q.CommitNonOpaqueTriangleHit();
			}
		} else if ( acc )
			q.CommitNonOpaqueTriangleHit();
	}
	const bool hit1 = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
	if ( !hit1 ) {   // a miss: sure unless an alpha decision on the way sat on its edge
		BakeRTHits[BAKERT_PC.first + k] = uint2( 0xFFFFFFFFu, why );
		return;
	}
	const float t0 = q.CommittedRayT();
	if ( any ) {
		if ( tri >= 0 ) {   // a sure alpha-tested hit answers it, whatever else was doubtful
			BakeRTHits[BAKERT_PC.first + k] = uint2( uint( tri ), 0u );
			return;
		}
		// an opaque commit: sure when its double re-test accepts it clear of every edge
		const uint p = BAKERT_PRIM_COMMIT( q );
		double tt;
		uint m2 = 0u;
		const bool acc = BakeRT_TestD( p, o, d, tmax, kBakeRTErr * ( oMag + double( t0 ) + 1.0L ), tt, m2 );
		BakeRTHits[BAKERT_PC.first + k] = acc && m2 == 0u ? uint2( uint( BakeRTRemap[p] ), 0u ) : uint2( 0xFFFFFFFFu, acc ? m2 : m2 | 32u );
		return;
	}

	// phase 2: everything in (0, t0 + the float error], each in double; nothing committed so nothing is culled
	why = 0u;
	const double eT = kBakeRTErr * ( oMag + double( t0 ) + 1.0L );
	const double eG = ( BAKERT_PC.flags & 2u ) != 0u ? 0.0L : kBakeRTGraze * ( oMag + double( t0 ) + 1.0L );
	const float tEnd = min( tmaxF, float( double( t0 ) * ( 1.0L + kBakeRTErr ) + eT + eG ) );
	RayDesc rd2;
	rd2.Origin = of;
	rd2.TMin = 0.0;
	rd2.Direction = df;
	rd2.TMax = tEnd;
	RayQuery<RAY_FLAG_NONE> q2;
	q2.TraceRayInline( BakeRTTLAS, RAY_FLAG_FORCE_NON_OPAQUE, 0xFF, rd2 );
	double best = 1e300L, second = 1e300L;
	int bestTri = -1;
	while ( q2.Proceed() ) {
		if ( q2.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE )
			continue;
		const uint p = BAKERT_PRIM_CAND( q2 );
		double tt;
		if ( !BakeRT_TestD( p, o, d, tmax, eT, tt, why ) )
			continue;
		const int st = BakeRTRemap[p];
		if ( tt < best ) {
			if ( st != bestTri )
				second = best;
			best = tt;
			bestTri = st;
		} else if ( st != bestTri && tt < second )
			second = tt;
	}
	if ( bestTri < 0 )
		why |= 32u;
#ifndef RQX_RED   // red: two accepted hits within the near pad no longer make the ray unsure
	else if ( second - best <= kBakeRTNearPad * ( 1.0L + best ) )
		why |= 16u;
#endif
	BakeRTHits[BAKERT_PC.first + k] = uint2( bestTri < 0 ? 0xFFFFFFFFu : uint( bestTri ), why );
}

#endif
