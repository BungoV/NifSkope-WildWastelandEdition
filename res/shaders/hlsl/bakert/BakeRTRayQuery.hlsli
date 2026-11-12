// Bake tracer: the RT tracer (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/bakert/rq.comp). One thread a ray, an
// inline ray query against the soup's BLAS (geometry 0 = the unmasked triangles, opaque; geometry 1 = the alpha-tested
// ones, tested here). Double-sided, nearest hit. Shared, no bindings: BakeRTRays, BakeRTHits, BakeRTTLAS, BakeRTRemap.
// Hits: t bits, soup triangle (0xFFFFFFFF = miss). Ray queries: DXIL cs_6_5.
#ifndef BAKERT_RAY_QUERY_HLSLI
#define BAKERT_RAY_QUERY_HLSLI

#include "BakeRTCommon.hlsli"

void BakeRT_RayQueryCS( uint k )
{
	if ( k >= BAKERT_PC.count )
		return;
	const BakeRTRay r = BakeRTRays[BAKERT_PC.first + k];
	RayDesc rd;
	rd.Origin = r.oT.xyz;
	rd.TMin = r.oT.w;
	rd.Direction = r.dT.xyz;
	rd.TMax = r.dT.w;
	RayQuery<RAY_FLAG_NONE> q;
	q.TraceRayInline( BakeRTTLAS, ( BAKERT_PC.flags & 1u ) != 0u ? RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH : RAY_FLAG_NONE, 0xFF, rd );
	while ( q.Proceed() ) {
		if ( q.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE )
			continue;
		const uint g = q.CandidateGeometryIndex();
		const uint p = q.CandidatePrimitiveIndex() + ( g == 1u ? BAKERT_PC.nOpaque : 0u );
		const float2 bc = q.CandidateTriangleBarycentrics();
#ifdef RQ_RED
		q.CommitNonOpaqueTriangleHit();   // red: the alpha hole is never tested
#else
		if ( !BakeRT_Hole( BakeRTRemap[p], bc.x, bc.y ) )
			q.CommitNonOpaqueTriangleHit();
#endif
	}
	uint2 h = uint2( 0u, 0xFFFFFFFFu );
	if ( q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ) {
		const uint g = q.CommittedGeometryIndex();
		const uint p = q.CommittedPrimitiveIndex() + ( g == 1u ? BAKERT_PC.nOpaque : 0u );
		h = uint2( asuint( q.CommittedRayT() ), uint( BakeRTRemap[p] ) );
	}
	BakeRTHits[BAKERT_PC.first + k] = h;
}

#endif
