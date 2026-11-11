#ifndef RTTRACESETRULE_H
#define RTTRACESETRULE_H

//! lane AIRPERF1 round 4: the ray-traced GI's traced set follows the streamed cells. Pure rules (no Qt), tested by
//! tests/cellstreamrule/run.sh. The store is rttraceset.cpp.

#include <cmath>
#include <cstdint>
#include <vector>

namespace rttracesetrule {

constexpr double kCell = 4096.0;
constexpr int kMaxRing = 16;         //!< the far terrain's reach round the block's centre, cells (the cap)
constexpr double kMinGapMs = 1500.0; //!< at most one publish (one BLAS build) in this time: a crossing is one publish

//! one traced unit: a loaded cell, or a large reference drawn near (lref)
struct Unit
{
	int x = 0, y = 0;
	std::uint32_t lref = 0;
	std::uint64_t tris = 0;
};

inline std::uint64_t mix( std::uint64_t h, std::uint64_t v )
{
	for ( int i = 0; i < 8; i++ ) {
		h = ( h ^ ( v & 0xffu ) ) * 0x100000001B3ULL;
		v >>= 8;
	}
	return h;
}

/*! what the traced geometry is made of: the units (the caller gives them sorted), the far ring and what it holds.
 *  The same loaded set gives the same key (no publish); the camera is not in it */
inline std::uint64_t setKey( const std::vector<Unit> & units, int ring, std::uint64_t farCells, std::uint64_t farTris )
{
#ifdef RTTRACESETRULE_RED_CONSTKEY
	(void)units; (void)ring; (void)farCells; (void)farTris;
	return 1;   // red: the set is published once and never again (the parked GI)
#else
	std::uint64_t h = 0xCBF29CE484222325ULL;
	h = mix( h, units.size() );
	for ( const Unit & u : units ) {
		h = mix( h, std::uint64_t( std::uint32_t( u.x ) ) );
		h = mix( h, std::uint64_t( std::uint32_t( u.y ) ) );
		h = mix( h, u.lref );
		h = mix( h, u.tris );
	}
	h = mix( h, std::uint64_t( std::uint32_t( ring ) ) );
	h = mix( h, farCells );
	h = mix( h, farTris );
	return h;
#endif
}

//! a changed set is merged and published now (else it waits: a merge runs, or the last publish is younger than the gap)
inline bool publishNow( bool changed, bool merging, bool everPublished, double msSincePublish, double minGapMs, bool redParked )
{
	if ( !changed || merging )
		return false;
	if ( !everPublished )
		return true;
	if ( redParked )
		return false;   // red WW_CELL_RTSET_RED=parked: the first set stays (the GI parked at the spawn)
	return msSincePublish >= minGapMs;
}

/*! the far terrain's ring round the block's centre, in cells (Chebyshev): the coarsest cascade's reach, one cell for
 *  the camera's distance from the centre and two of margin for the rays of the outer probes; never inside the block */
inline int farRing( double reachUnits, int gridHalf )
{
	int r = reachUnits > 0.0 ? int( std::ceil( reachUnits / kCell ) ) + 3 : gridHalf + 3;
	if ( r < gridHalf + 1 )
		r = gridHalf + 1;
	return r > kMaxRing ? kMaxRing : r;
}

inline bool inRing( int x, int y, int cx, int cy, int ring )
{
	const int dx = x > cx ? x - cx : cx - x, dy = y > cy ? y - cy : cy - y;
	return ( dx > dy ? dx : dy ) <= ring;
}

//! the traced set's box in x, y (world units): the cells within `half` of the centre
inline void setBox( int cx, int cy, int half, double lo[2], double hi[2] )
{
	lo[0] = double( cx - half ) * kCell;
	lo[1] = double( cy - half ) * kCell;
	hi[0] = double( cx + half + 1 ) * kCell;
	hi[1] = double( cy + half + 1 ) * kCell;
}

inline bool boxHolds( const double lo[2], const double hi[2], double x, double y )
{
	return x >= lo[0] && x <= hi[0] && y >= lo[1] && y <= hi[1];
}

//! the camera's cell is one of the set's cells
inline bool cameraIn( const std::vector<Unit> & units, int ex, int ey )
{
	for ( const Unit & u : units )
		if ( !u.lref && u.x == ex && u.y == ey )
			return true;
	return false;
}

} // namespace rttracesetrule

#endif
