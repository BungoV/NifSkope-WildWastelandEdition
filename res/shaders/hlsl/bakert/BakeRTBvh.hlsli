// Bake tracer: the no-RT tracer (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/bakert/bvh.comp). One thread a ray walks
// src/probebvh.h's tree exactly as Bvh::ray does (same node order, same stack cap, same "t <= best" tie rule), in float.
// Shared, no bindings: BakeRTRays, BakeRTHits, BakeRTNodes, BakeRTIdx, BakeRTTris. Hits: t bits, soup triangle.
#ifndef BAKERT_BVH_HLSLI
#define BAKERT_BVH_HLSLI

#include "BakeRTCommon.hlsli"

float BakeRT_Nf( uint i ) { return asfloat( BakeRTNodes[i] ); }

bool BakeRT_Slab( uint n, float3 o, float3 inv, float tmax )
{
	float t0 = 0.0, t1 = tmax;
	for ( int k = 0; k < 3; k++ ) {
		const float lo = BakeRT_Nf( n * 9u + uint( k ) ), hi = BakeRT_Nf( n * 9u + 3u + uint( k ) );
		if ( inv[k] == 0.0 ) {   // flat in this axis: the box holds the ray or not
			if ( o[k] < lo || o[k] > hi )
				return false;
			continue;
		}
		float a = ( lo - o[k] ) * inv[k];
		float b = ( hi - o[k] ) * inv[k];
		if ( a > b ) {
			const float s = a;
			a = b;
			b = s;
		}
		t0 = max( t0, a );
		t1 = min( t1, b );
		if ( t0 > t1 )
			return false;
	}
	return true;
}

// MIRROR probebvh.h Bvh::tri (in float): soup triangle t; u weighs vertex 1, v vertex 2
bool BakeRT_Tri( uint t, float3 o, float3 d, out float tOut, out float u, out float v )
{
	const uint b = t * 9u;
	const float3 p0 = float3( BakeRTTris[b], BakeRTTris[b + 1u], BakeRTTris[b + 2u] );
	const float3 e1 = float3( BakeRTTris[b + 3u], BakeRTTris[b + 4u], BakeRTTris[b + 5u] ) - p0;
	const float3 e2 = float3( BakeRTTris[b + 6u], BakeRTTris[b + 7u], BakeRTTris[b + 8u] ) - p0;
	tOut = 0.0;
	u = 0.0;
	v = 0.0;
	const float3 pv = cross( d, e2 );
	const float det = dot( e1, pv );
	if ( abs( det ) < 1e-12 )
		return false;
	const float id = 1.0 / det;
	const float3 tvv = o - p0;
	u = dot( tvv, pv ) * id;
	bool wwh0 = u < 0.0;
	bool wwh1 = u > 1.0;
	if (wwh0 || wwh1)
		return false;
	const float3 qv = cross( tvv, e1 );
	v = dot( d, qv ) * id;
	bool wwh2 = v < 0.0;
	bool wwh3 = u + v > 1.0;
	if (wwh2 || wwh3)
		return false;
	tOut = dot( e2, qv ) * id;
	return true;
}

void BakeRT_BvhCS( uint k )
{
	if ( k >= BAKERT_PC.count )
		return;
	const BakeRTRay r = BakeRTRays[BAKERT_PC.first + k];
	const float3 o = r.oT.xyz, d = r.dT.xyz;
	const float tmin = r.oT.w;
	float3 inv;
	for ( int a = 0; a < 3; a++ )
		inv[a] = d[a] != 0.0 ? 1.0 / d[a] : 0.0;
	float best = r.dT.w;
	int hitTri = -1;
	uint stack[128];
	int sp = 0;
	stack[sp++] = 0u;
	const bool anyHit = ( BAKERT_PC.flags & 1u ) != 0u;
	while ( sp > 0 && !( anyHit && hitTri >= 0 ) ) {
		const uint n = stack[--sp];
		if ( !BakeRT_Slab( n, o, inv, best ) )
			continue;
		const int left = int( BakeRTNodes[n * 9u + 6u] );
		const int first = int( BakeRTNodes[n * 9u + 7u] );
		const int count = int( BakeRTNodes[n * 9u + 8u] );
		if ( left < 0 ) {
			for ( int i = first; i < first + count; i++ ) {
				const int t = BakeRTIdx[i];
				float tt, bu, bv;
#ifdef BVH_RED
				if ( BakeRT_Tri( uint( t ), o, d, tt, bu, bv ) && tt > tmin && tt <= best ) {   // red: the alpha hole is never tested
#else
				if ( BakeRT_Tri( uint( t ), o, d, tt, bu, bv ) && tt > tmin && tt <= best && !BakeRT_Hole( t, bu, bv ) ) {
#endif
					best = tt;
					hitTri = t;
				}
			}
		} else if ( sp < 126 ) {
			stack[sp++] = uint( first );   // right
			stack[sp++] = uint( left );
		}
	}
	BakeRTHits[BAKERT_PC.first + k] = hitTri >= 0 ? uint2( asuint( best ), uint( hitTri ) ) : uint2( 0u, 0xFFFFFFFFu );
}

#endif
