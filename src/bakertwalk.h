#ifndef BAKERTWALK_H
#define BAKERTWALK_H

/* Lane RTBAKE1FIX (2026-10-22): the CPU walk refine() runs for a GPU nearest hit, header-only so the offline gate
 * (tests/bakert_walk_gate.cpp) runs the very same code.
 *
 * probebvh::Bvh::ray() started at the GPU's t prunes more than the full walk does: a node whose box entry rounds a
 * hair past that t is skipped, though the full walk (its running best still larger there) visits it and can keep a
 * different tie or a nearer hit. That flipped one probe's noise test in the in-cell museum bake (one batch more on
 * rt). rayFrom() walks like ray() but reports when any node or triangle it left out lies within `pad` past its
 * running best: only such a near miss can make the two walks differ, so *sure = true means ray()'s own answer. */

#include "probebvh.h"

namespace bakert {

// the near margin, relative to (1 + t): far above double rounding of a box entry or a hit t, far below a real gap
constexpr double kNearPad = 1e-6;

inline bool rayFrom( const probebvh::Bvh & B, const double o[3], const double d[3], double tmax, double * tHit, int * triOut,
	bool * sure )
{
	*sure = true;
	if ( B.nodes.empty() )
		return false;
	double inv[3];
	for ( int k = 0; k < 3; k++ )
		inv[k] = std::fabs( d[k] ) > 1e-300 ? 1.0 / d[k] : 0.0;
	double best = tmax;
	bool hit = false;
	const bool useMask = B.mask && !B.mask->empty();
	int hitTri = -1;
	int stack[128];
	int sp = 0;
	stack[sp++] = 0;
	while ( sp ) {
		const probebvh::Bvh::Node & nd = B.nodes[size_t( stack[--sp] )];
		if ( !probebvh::Bvh::slab( nd, o, inv, best ) ) {
			if ( probebvh::Bvh::slab( nd, o, inv, best + kNearPad * ( 1.0 + best ) ) ) {
				*sure = false;   // left out by a hair: the full walk may visit it
				return false;
			}
			continue;
		}
		if ( nd.left < 0 ) {
			for ( int i = nd.first; i < nd.first + nd.count; i++ ) {
				double tt, bu, bv;
				const int ti = B.idx[size_t( i )];
				if ( !probebvh::Bvh::tri( &B.t[size_t( ti ) * 9], o, d, tt, bu, bv ) || !( tt > 1e-4 ) )
					continue;
				if ( tt > best ) {
					if ( tt <= best + kNearPad * ( 1.0 + best ) && !( useMask && B.mask->hole( ti, bu, bv ) ) ) {
						*sure = false;   // a near tie the bound decided
						return false;
					}
					continue;
				}
				if ( useMask && B.mask->hole( ti, bu, bv ) )
					continue;
				best = tt;
				hit = true;
				hitTri = ti;
			}
		} else if ( sp < 126 ) {
			stack[sp++] = nd.first;
			stack[sp++] = nd.left;
		} else {
			*sure = false;   // the stack cap drops nodes, differently from the full walk
			return false;
		}
	}
	if ( hit && tHit )
		*tHit = best;
	if ( hit && triOut )
		*triOut = hitTri;
	return hit;
}

/* Lane GIBAKE2: triangle t (9 floats) grown in its own plane so each edge moves out by `del`; a vertex moves
 * del (ma + mb) / (1 + ma.mb) (ma, mb its two edges' outward normals), capped at 1000 del on a sliver's sharp corner.
 * Returns false (out = in) for a degenerate triangle; *capped counts capped corners. */
inline bool growTri( const float * in, double del, float * out, int * capped )
{
	double p[3][3];
	for ( int i = 0; i < 9; i++ ) {
		p[i / 3][i % 3] = in[i];
		out[i] = in[i];
	}
	auto sub = []( const double * a, const double * b, double * r ) { for ( int k = 0; k < 3; k++ ) r[k] = a[k] - b[k]; };
	auto crs = []( const double * a, const double * b, double * r ) {
		r[0] = a[1] * b[2] - a[2] * b[1];
		r[1] = a[2] * b[0] - a[0] * b[2];
		r[2] = a[0] * b[1] - a[1] * b[0];
	};
	auto len = []( const double * a ) { return std::sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] ); };
	double e1[3], e2[3], n[3];
	sub( p[1], p[0], e1 );
	sub( p[2], p[0], e2 );
	crs( e1, e2, n );
	const double a2 = len( n );
	if ( !( a2 > 1e-12 ) )
		return false;   // degenerate: the double test refuses it anyway
	for ( double & c : n )
		c /= a2;
	double m[3][3];   // edge j (p[j] -> p[j+1]): its outward unit normal in the plane
	for ( int j = 0; j < 3; j++ ) {
		double e[3];
		sub( p[( j + 1 ) % 3], p[j], e );
		crs( e, n, m[j] );
		const double l = len( m[j] );
		if ( !( l > 0.0 ) )
			return false;
		for ( double & c : m[j] )
			c /= l;
	}
	for ( int i = 0; i < 3; i++ ) {
		const double * ma = m[( i + 2 ) % 3], * mb = m[i];
		const double den = std::max( 1.0 + ma[0] * mb[0] + ma[1] * mb[1] + ma[2] * mb[2], 1e-30 );
		double x[3];
		for ( int k = 0; k < 3; k++ )
			x[k] = ( ma[k] + mb[k] ) * del / den;
		const double xl = len( x );
		if ( xl > 1000.0 * del ) {
			if ( capped )
				( *capped )++;
			for ( double & c : x )
				c *= 1000.0 * del / xl;
		}
		for ( int k = 0; k < 3; k++ )
			out[i * 3 + k] = float( p[i][k] + x[k] );
	}
	return true;
}

} // namespace bakert

#endif // BAKERTWALK_H
