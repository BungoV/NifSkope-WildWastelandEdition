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

} // namespace bakert

#endif // BAKERTWALK_H
