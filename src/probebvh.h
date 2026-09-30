/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENSE BLOCK *****/

#ifndef PROBEBVH_H
#define PROBEBVH_H

/* THE PROBE RAY TRACER (lane PRTPPLACE's BVH, moved here by lane PRTPBAKE so the
 * bake casts the same rays the placer does). Header-only; every file that
 * includes it gets the FMA contraction off below, which the placer's gate needs:
 * a ray that grazes a triangle edge must land on the same side on every CPU and
 * in the gate's Python re-trace (tests/spells/probe_place.py). */
#if defined( __GNUC__ ) && !defined( __clang__ )
#pragma GCC optimize( "fp-contract=off" )
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace probebvh {

// ------------------------------------------------------------------ the BVH
/* Median split on the longest centroid axis, 4 triangles a leaf. The soup is
 * stored relative to a local origin (the rect's center) so float precision
 * does not depend on how far from the world origin the block is; the ray test
 * itself runs in double. Double-sided: the game's physics pick hits a face
 * from either side, and so must this. */
struct Bvh
{
	std::vector<float> t;      // 9 floats a triangle, local
	struct Node
	{
		float lo[3], hi[3];
		int left = -1;          // interior: right = left + 1
		int first = 0, count = 0;
	};
	std::vector<Node> nodes;
	std::vector<int> idx;
	std::vector<float> cen;

	void bounds( int b, int e, float lo[3], float hi[3] ) const
	{
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = 3.4e38f;
			hi[k] = -3.4e38f;
		}
		for ( int i = b; i < e; i++ ) {
			const float * p = &t[size_t( idx[i] ) * 9];
			for ( int v = 0; v < 3; v++ )
				for ( int k = 0; k < 3; k++ ) {
					lo[k] = std::min( lo[k], p[v * 3 + k] );
					hi[k] = std::max( hi[k], p[v * 3 + k] );
				}
		}
	}

	int buildRange( int b, int e )
	{
		const int ni = int( nodes.size() );
		nodes.push_back( Node() );
		float lo[3], hi[3];
		bounds( b, e, lo, hi );
		std::memcpy( nodes[ni].lo, lo, sizeof lo );
		std::memcpy( nodes[ni].hi, hi, sizeof hi );
		if ( e - b <= 4 ) {
			nodes[ni].first = b;
			nodes[ni].count = e - b;
			return ni;
		}
		float clo[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, chi[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
		for ( int i = b; i < e; i++ )
			for ( int k = 0; k < 3; k++ ) {
				clo[k] = std::min( clo[k], cen[size_t( idx[i] ) * 3 + k] );
				chi[k] = std::max( chi[k], cen[size_t( idx[i] ) * 3 + k] );
			}
		int ax = 0;
		for ( int k = 1; k < 3; k++ )
			if ( chi[k] - clo[k] > chi[ax] - clo[ax] )
				ax = k;
		const int mid = ( b + e ) / 2;
		std::nth_element( idx.begin() + b, idx.begin() + mid, idx.begin() + e,
			[&]( int x, int y ) { return cen[size_t( x ) * 3 + ax] < cen[size_t( y ) * 3 + ax]; } );
		const int l = buildRange( b, mid );
		const int r = buildRange( mid, e );
		nodes[ni].left = l;
		nodes[ni].count = 0;
		nodes[ni].first = r;  // interior nodes keep the right child here
		return ni;
	}

	void build()
	{
		const int n = int( t.size() / 9 );
		idx.resize( size_t( n ) );
		cen.resize( size_t( n ) * 3 );
		for ( int i = 0; i < n; i++ ) {
			idx[size_t( i )] = i;
			for ( int k = 0; k < 3; k++ )
				cen[size_t( i ) * 3 + k] = ( t[size_t( i ) * 9 + k] + t[size_t( i ) * 9 + 3 + k]
					+ t[size_t( i ) * 9 + 6 + k] ) / 3.0f;
		}
		nodes.reserve( size_t( n / 2 + 16 ) );
		if ( n )
			buildRange( 0, n );
		std::vector<float>().swap( cen );
	}

	static bool slab( const Node & nd, const double o[3], const double inv[3], double tmax )
	{
		double t0 = 0.0, t1 = tmax;
		for ( int k = 0; k < 3; k++ ) {
			if ( inv[k] == 0.0 ) {
				/* The ray runs flat in this axis: the box holds it or not. (Scaling
				 * by a huge inverse instead turns a ray lying ON the box face --
				 * a probe at z 120 beside trim that tops out at 120 -- into
				 * 0 * 1e300 = 0 and closes the interval.) */
				if ( o[k] < double( nd.lo[k] ) || o[k] > double( nd.hi[k] ) )
					return false;
				continue;
			}
			double a = ( double( nd.lo[k] ) - o[k] ) * inv[k];
			double b = ( double( nd.hi[k] ) - o[k] ) * inv[k];
			if ( a > b )
				std::swap( a, b );
			t0 = std::max( t0, a );
			t1 = std::min( t1, b );
			if ( t0 > t1 )
				return false;
		}
		return true;
	}

	static bool tri( const float * p, const double o[3], const double d[3], double & tOut )
	{
		const double e1[3] = { p[3] - double( p[0] ), p[4] - double( p[1] ), p[5] - double( p[2] ) };
		const double e2[3] = { p[6] - double( p[0] ), p[7] - double( p[1] ), p[8] - double( p[2] ) };
		const double pv[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2],
			d[0] * e2[1] - d[1] * e2[0] };
		const double det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
		if ( std::fabs( det ) < 1e-12 )
			return false;
		const double id = 1.0 / det;
		const double tv[3] = { o[0] - p[0], o[1] - p[1], o[2] - p[2] };
		const double u = ( tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2] ) * id;
		if ( u < 0.0 || u > 1.0 )
			return false;
		const double qv[3] = { tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2],
			tv[0] * e1[1] - tv[1] * e1[0] };
		const double v = ( d[0] * qv[0] + d[1] * qv[1] + d[2] * qv[2] ) * id;
		if ( v < 0.0 || u + v > 1.0 )
			return false;
		tOut = ( e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2] ) * id;
		return true;
	}

	//! Nearest hit along a unit direction within (1e-4, tmax]; `triOut` gets the soup triangle's index.
	bool ray( const double o[3], const double d[3], double tmax, double * tHit, int * triOut = nullptr ) const
	{
		if ( nodes.empty() )
			return false;
		double inv[3];
		for ( int k = 0; k < 3; k++ )
			inv[k] = std::fabs( d[k] ) > 1e-300 ? 1.0 / d[k] : 0.0;   // 0 = flat in this axis
		double best = tmax;
		bool hit = false;
		int hitTri = -1;
		int stack[128];
		int sp = 0;
		stack[sp++] = 0;
		while ( sp ) {
			const Node & nd = nodes[size_t( stack[--sp] )];
			if ( !slab( nd, o, inv, best ) )
				continue;
			if ( nd.left < 0 ) {
				for ( int i = nd.first; i < nd.first + nd.count; i++ ) {
					double tt;
					if ( tri( &t[size_t( idx[size_t( i )] ) * 9], o, d, tt ) && tt > 1e-4 && tt <= best ) {
						best = tt;
						hit = true;
						hitTri = idx[size_t( i )];
					}
				}
			} else if ( sp < 126 ) {
				stack[sp++] = nd.first;  // right
				stack[sp++] = nd.left;
			}
		}
		if ( hit && tHit )
			*tHit = best;
		if ( hit && triOut )
			*triOut = hitTri;
		return hit;
	}
};

} // namespace probebvh

#endif // PROBEBVH_H
