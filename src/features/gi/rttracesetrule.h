#ifndef RTTRACESETRULE_H
#define RTTRACESETRULE_H

//! lane AIRPERF1 round 4: the ray-traced GI's traced set follows the streamed cells. Pure rules (no Qt), tested by
//! tests/cellstreamrule/run.sh. The store is rttraceset.cpp.

#include <algorithm>
#include <cmath>
#include <cstddef>
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
inline std::uint64_t setKey( const std::vector<Unit> & units, int ring, std::uint64_t farCells, std::uint64_t farTris,
	std::uint64_t farKey = 0 )   // round 5: which far runs (shape, run) the set holds
{
#ifdef RTTRACESETRULE_RED_CONSTKEY
	(void)units; (void)ring; (void)farCells; (void)farTris; (void)farKey;
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
	h = mix( h, farKey );
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

// ---- round 5: the far objects in the set, the warm hand-over, the soup's frame

constexpr std::uint64_t kFarObjTriCap = 600000;   //!< the far objects' triangles in one set (the budget; ~22 MB kept)

//! a far cell's object triangles, for the budget
struct FarCell
{
	int x = 0, y = 0;
	std::uint64_t tris = 0;
};

/*! the far objects' budget: the cells sorted nearest the centre first (Chebyshev, then y, then x) and kept while the
 *  running sum fits; the first cell that does not fit ends the list, so no far cell is traced past a nearer one that is
 *  not. Returns how many of the sorted `cells` are kept; `dropped` takes the triangles left out */
inline size_t farCap( std::vector<FarCell> & cells, int cx, int cy, std::uint64_t cap, std::uint64_t * dropped )
{
	const auto dist = [cx, cy]( const FarCell & c ) {
		const int dx = c.x > cx ? c.x - cx : cx - c.x, dy = c.y > cy ? c.y - cy : cy - c.y;
		return dx > dy ? dx : dy;
	};
	std::sort( cells.begin(), cells.end(), [&dist]( const FarCell & a, const FarCell & b ) {
		const int da = dist( a ), db = dist( b );
		if ( da != db )
			return da < db;
		return a.y != b.y ? a.y < b.y : a.x < b.x;
	} );
	size_t keep = 0;
#ifdef RTTRACESETRULE_RED_NOCAP
	(void)cap;
	keep = cells.size();   // red: no budget
#else
	std::uint64_t sum = 0;
	for ( ; keep < cells.size(); keep++ ) {
		if ( sum + cells[keep].tris > cap )
			break;
		sum += cells[keep].tris;
	}
#endif
	if ( dropped ) {
		*dropped = 0;
		for ( size_t i = keep; i < cells.size(); i++ )
			*dropped += cells[i].tris;
	}
	return keep;
}

/*! an instanced far row is traced: the ring's own row (its ladder step is 0, or carries 0x80). The other rows of a
 *  bucket are the tree ladder's steps of the same tree: tracing them too would stack up to four copies in one place */
inline bool rowTraced( unsigned step )
{
#ifdef RTTRACESETRULE_RED_ALLROWS
	(void)step;
	return true;
#else
	return step == 0 || ( step & 0x80u ) != 0;
#endif
}

/*! a far run is left out because a loaded cell already traces it: a placed object (ref) when a loaded cell lists that
 *  ref, wherever its row stands (the far field's own hide rule, cellfarhide.cpp rowHidden); geometry without a ref
 *  when its cell is loaded */
inline bool farSkip( std::uint32_t ref, bool refListed, bool cellLoaded )
{
#ifdef RTTRACESETRULE_RED_BYCELLONLY
	(void)ref; (void)refListed;
	return cellLoaded;
#else
	return ref ? refListed : cellLoaded;
#endif
}

/*! the updates a new volume runs before it is drawn in place of the old one (the hand-over's bar): the smallest count
 *  at which a probe's own hysteresis age / (age + 1) has reached the base `h`, so every probe blends as a settled one
 *  does and each texel is the mean of that many updates (rtddgimath.h kHysteresis 0.97: 33) */
inline int warmUpdates( double h )
{
#ifdef RTTRACESETRULE_RED_NOWARM
	(void)h;
	return 0;   // red: the new volume is drawn after its first frame's updates (the pop)
#else
	for ( int n = 1; n < 512; n++ )
		if ( double( n ) / double( n + 1 ) >= h )
			return n;
	return 512;
#endif
}

/*! the soup's frame is the world's: a cell's land starts at cell * 4096 whatever the document's origin is, so a cell
 *  gathered in the open's block and the same cell streamed in after a walk (another origin) give the same triangles.
 *  The far shapes carry the origin in their translation (docWorld), so they meet the soup in the same frame */
inline double soupCellOrigin( int cell, double docOrigin )
{
#ifdef RTTRACESETRULE_RED_FRAMEREL
	return double( cell ) * kCell - docOrigin;   // red: origin-relative (two opens disagree)
#else
	(void)docOrigin;
	return double( cell ) * kCell;
#endif
}

inline double docWorld( double rel, double docOrigin )
{
	return rel + docOrigin;
}

} // namespace rttracesetrule

#endif
