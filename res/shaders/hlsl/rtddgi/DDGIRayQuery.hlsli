// DDGI volume: the inline ray queries the traces share (lane VKCOMPUTE1, 2026-11-12; from trace.comp ddgiPrim /
// ddgiTriTest / ddgiBackFace / ddgiShadow, restir.comp rsBlocked, airrays.comp arBlocked). Shared, no bindings: it reads
// DDGITLAS, DDGIVerts, DDGIRemap, DDGITriAttr by name (the shim declares them; DDGI_USE_TLAS). Ray queries: DXIL cs_6_5.
// The BLAS is built from GROWN triangles, so every candidate is re-tested here on its TRUE triangle (probebvh's rule).
#ifndef DDGI_RAY_QUERY_HLSLI
#define DDGI_RAY_QUERY_HLSLI

#include "DDGIResources.hlsli"

// BLAS (instance custom index c, geometry g, primitive i) -> its row in DDGIRemap / DDGIVerts (geometry 1 follows the
// counts.w opaque rows). MIRROR rttracesetrule.h blasRow
uint DDGI_Prim( uint c, uint g, uint i ) { return c + i + ( g == 1u ? uint( DDGI_P.counts.w ) : 0u ); }

uint DDGI_CandidatePrim( RayQuery<RAY_FLAG_NONE> q )
{
	return DDGI_Prim( q.CandidateInstanceID(), q.CandidateGeometryIndex(), q.CandidatePrimitiveIndex() );
}

// MIRROR probebvh.h Bvh::tri (in float): the ray against BLAS row pr's TRUE triangle; u weighs vertex 1, v vertex 2
bool DDGI_TriTest( uint pr, float3 o, float3 d, out float t, out float u, out float v )
{
	t = 0.0;
	u = 0.0;
	v = 0.0;
	const uint vb = pr * 9u;
	const float3 p0 = float3( DDGIVerts[vb], DDGIVerts[vb + 1u], DDGIVerts[vb + 2u] );
	const float3 e1 = float3( DDGIVerts[vb + 3u], DDGIVerts[vb + 4u], DDGIVerts[vb + 5u] ) - p0;
	const float3 e2 = float3( DDGIVerts[vb + 6u], DDGIVerts[vb + 7u], DDGIVerts[vb + 8u] ) - p0;
	const float3 pv = cross( d, e2 );
	const float det = dot( e1, pv );
	if ( abs( det ) < 1e-12 )
		return false;
	const float id = 1.0 / det;
	const float3 tv = o - p0;
	u = dot( tv, pv ) * id;
	bool wwh0 = u < 0.0;
	bool wwh1 = u > 1.0;
	if (wwh0 || wwh1)
		return false;
	const float3 qv = cross( tv, e1 );
	v = dot( d, qv ) * id;
	bool wwh2 = v < 0.0;
	bool wwh3 = u + v > 1.0;
	if (wwh2 || wwh3)
		return false;
	t = dot( e2, qv ) * id;
	return true;
}

// MIRROR src/rtddgi.cpp Twin::backFace: a one-sided face seen from behind
bool DDGI_BackFace( uint pr, float3 d )
{
	const uint vb = pr * 9u;
	const float3 v0 = float3( DDGIVerts[vb], DDGIVerts[vb + 1u], DDGIVerts[vb + 2u] );
	const float3 e1 = float3( DDGIVerts[vb + 3u], DDGIVerts[vb + 4u], DDGIVerts[vb + 5u] ) - v0;
	const float3 e2 = float3( DDGIVerts[vb + 6u], DDGIVerts[vb + 7u], DDGIVerts[vb + 8u] ) - v0;
	const float3 nf = normalize( cross( e1, e2 ) );
	return DDGITriAttr[uint( DDGIRemap[pr] ) * 2u].w < 0.5 && dot( nf, d ) > 0.0;
}

// MIRROR probebvh.h Bvh::anyHitIn (tLo 1e-4, every face picked): is anything hit in (1e-4, tmax]?
// hw: the trace's red DDGI_RED_HWHIT (the hardware's hit, its barycentrics); noHole: holes block (ReSTIR / air red)
bool DDGI_AnyHit( float3 o, float3 d, float tmax, bool hw, bool noHole )
{
	if ( tmax <= 1e-3 )
		return false;
	RayDesc r;
	r.Origin = o;
	r.TMin = 0.0;
	r.Direction = d;
	r.TMax = tmax;
	RayQuery<RAY_FLAG_NONE> q;
	q.TraceRayInline( DDGITLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | ( hw ? RAY_FLAG_NONE : RAY_FLAG_FORCE_NON_OPAQUE ), 0xFF, r );
	while ( q.Proceed() ) {
		if ( q.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE )
			continue;
		const uint pr = DDGI_CandidatePrim( q );
		if ( hw ) {
			const float2 bc = q.CandidateTriangleBarycentrics();
			if ( !DDGI_Hole( DDGIRemap[pr], bc.x, bc.y ) )
				q.CommitNonOpaqueTriangleHit();
			continue;
		}
		float t, u, v;
		if ( DDGI_TriTest( pr, o, d, t, u, v ) && t > DDGI_TMIN && t <= tmax && ( noHole || !DDGI_Hole( DDGIRemap[pr], u, v ) ) )
			q.CommitNonOpaqueTriangleHit();
	}
	return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

#endif
