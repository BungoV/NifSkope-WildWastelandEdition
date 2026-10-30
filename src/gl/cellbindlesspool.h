#ifndef CELLBINDLESSPOOL_H
#define CELLBINDLESSPOOL_H

/* lane BINDLESS2: the geometry pool's bookkeeping, apart from GL so tests/bindlesspool can run it alone.
 * One arena per vertex layout: vertices and 16-bit indices are appended; a shape draws its span with a base vertex
 * (its indices stay the shape's own) and an index byte offset. A full arena is rebuilt packed (garbage dropped) at
 * twice the size when more than half of it is still live.
 * lane BINDREOPEN1: that arena never gave memory back (a 22-step stream walk: 84 -> 2496 MB). Now a released span goes
 * to a grave stamped with the frame that last could read it; once that frame's fence has passed (`retire`) its vertex
 * and index ranges join free lists (coalesced, the tail given back to the append point) and are reused best-fit. A
 * rebuild sizes the arena from its live part (1.5x, 4096-element steps, the old floors), so it also SHRINKS: the
 * frame sweep re-makes an arena whose live share fell under half (`shrinkWanted`). Red WW_BINDLESS_RED=poolgrow
 * (`grow`): the old append-only rule above. tests/bindreopen1 runs both over a stream walk. */

#include <cstdint>
#include <iterator>
#include <map>
#include <vector>

namespace WwBlPool
{
struct Span
{
	std::uint32_t firstVert = 0, numVerts = 0;
	std::uint32_t firstIndex = 0, numIndices = 0;
};

//! lane BINDREOPEN1: the free ranges of one buffer (first -> count), adjacent ranges merged
struct FreeList
{
	std::map<std::uint32_t, std::uint32_t> runs;
	std::uint64_t total = 0;

	//! the smallest run that holds `n` (its front), or false
	bool take( std::uint32_t n, std::uint32_t & at )
	{
		if ( !n ) {
			at = 0;
			return true;
		}
		auto best = runs.end();
		for ( auto it = runs.begin(); it != runs.end(); ++it )
			if ( it->second >= n && ( best == runs.end() || it->second < best->second ) )
				best = it;
		if ( best == runs.end() )
			return false;
		at = best->first;
		const std::uint32_t left = best->second - n;
		runs.erase( best );
		if ( left )
			runs[at + n] = left;
		total -= n;
		return true;
	}
	void give( std::uint32_t at, std::uint32_t n )
	{
		if ( !n )
			return;
		total += n;
		auto next = runs.lower_bound( at );
		if ( next != runs.begin() ) {
			auto prev = std::prev( next );
			if ( prev->first + prev->second == at ) {
				at = prev->first;
				n += prev->second;
				runs.erase( prev );
			}
		}
		if ( next != runs.end() && at + n == next->first ) {
			n += next->second;
			runs.erase( next );
		}
		runs[at] = n;
	}
	//! the run ending at `end` is cut off (the append point moves back over it): its first, or `end`
	std::uint32_t trimTail( std::uint32_t end )
	{
		if ( runs.empty() )
			return end;
		auto last = std::prev( runs.end() );
		if ( last->first + last->second != end )
			return end;
		const std::uint32_t at = last->first;
		total -= last->second;
		runs.erase( last );
		return at;
	}
	void clear()
	{
		runs.clear();
		total = 0;
	}
};

struct Grave
{
	Span s;
	std::uint64_t epoch = 0;	// the frame whose fence frees it
};

struct Arena
{
	std::uint32_t vertCap = 0, idxCap = 0;	// in vertices / indices
	std::uint32_t vertUsed = 0, idxUsed = 0;	// the append points
	std::uint32_t vertGarbage = 0, idxGarbage = 0;	// the red's released elements (reclaimed by a rebuild only)
	bool grow = false;	// lane BINDREOPEN1: red poolgrow, the append-only arena
	FreeList fv, fi;
	std::vector<Grave> graves;
	std::uint64_t graveVerts = 0, graveIdx = 0;
	std::uint64_t reused = 0;	// spans placed (in part) from a free list

	static constexpr std::uint32_t kFloorVerts = 65536, kFloorIdx = 196608;

	//! a span (free ranges first, then the append point), or false when it does not fit (the caller rebuilds)
	bool alloc( std::uint32_t verts, std::uint32_t idx, Span & s )
	{
		if ( grow ) {
			if ( std::uint64_t( vertUsed ) + verts > vertCap || std::uint64_t( idxUsed ) + idx > idxCap )
				return false;
			s.firstVert = vertUsed;
			s.numVerts = verts;
			s.firstIndex = idxUsed;
			s.numIndices = idx;
			vertUsed += verts;
			idxUsed += idx;
			return true;
		}
		std::uint32_t v = 0, i = 0;
		const bool fromV = fv.take( verts, v );
		if ( !fromV && std::uint64_t( vertUsed ) + verts > vertCap )
			return false;
		const bool fromI = fi.take( idx, i );
		if ( !fromI && std::uint64_t( idxUsed ) + idx > idxCap ) {
			if ( fromV )
				fv.give( v, verts );
			return false;
		}
		if ( !fromV ) {
			v = vertUsed;
			vertUsed += verts;
		}
		if ( !fromI ) {
			i = idxUsed;
			idxUsed += idx;
		}
		if ( ( fromV && verts ) || ( fromI && idx ) )
			reused++;
		s.firstVert = v;
		s.numVerts = verts;
		s.firstIndex = i;
		s.numIndices = idx;
		return true;
	}
	//! `epoch`: the last frame that may still read the span (its fence frees it in `retire`)
	void release( const Span & s, std::uint64_t epoch = 0 )
	{
		if ( grow ) {
			vertGarbage += s.numVerts;
			idxGarbage += s.numIndices;
			return;
		}
		graves.push_back( Grave{ s, epoch } );
		graveVerts += s.numVerts;
		graveIdx += s.numIndices;
	}
	//! every grave whose frame is done (epoch <= doneEpoch) gives its ranges to the free lists
	void retire( std::uint64_t doneEpoch )
	{
		if ( grow || graves.empty() )
			return;
		std::size_t k = 0;
		for ( const Grave & g : graves ) {
			if ( g.epoch <= doneEpoch ) {
				fv.give( g.s.firstVert, g.s.numVerts );
				fi.give( g.s.firstIndex, g.s.numIndices );
				graveVerts -= g.s.numVerts;
				graveIdx -= g.s.numIndices;
			} else {
				graves[k++] = g;
			}
		}
		graves.resize( k );
		vertUsed = fv.trimTail( vertUsed );
		idxUsed = fi.trimTail( idxUsed );
	}
	std::uint64_t liveVerts() const
	{
		return grow ? std::uint64_t( vertUsed - vertGarbage ) : std::uint64_t( vertUsed ) - fv.total - graveVerts;
	}
	std::uint64_t liveIdx() const
	{
		return grow ? std::uint64_t( idxUsed - idxGarbage ) : std::uint64_t( idxUsed ) - fi.total - graveIdx;
	}
	//! the capacities a rebuild takes so that `verts`/`idx` more fit
	void plan( std::uint32_t verts, std::uint32_t idx, std::uint32_t & newVertCap, std::uint32_t & newIdxCap ) const
	{
		const std::uint64_t liveV = liveVerts() + verts;
		const std::uint64_t liveI = liveIdx() + idx;
		if ( grow ) {	// the red: packed, doubled until it fits (64k/192k floor), never smaller
			std::uint64_t v = vertCap ? vertCap : kFloorVerts, i = idxCap ? idxCap : kFloorIdx;
			while ( v < liveV * 2 && v < ( 1ULL << 31 ) )
				v *= 2;
			while ( i < liveI * 2 && i < ( 1ULL << 31 ) )
				i *= 2;
			newVertCap = std::uint32_t( v );
			newIdxCap = std::uint32_t( i );
			return;
		}
		auto size = []( std::uint64_t live, std::uint64_t floor ) {
			std::uint64_t c = ( live * 3 + 1 ) / 2;
			c = ( c + 4095 ) & ~std::uint64_t( 4095 );
			if ( c < floor )
				c = floor;
			if ( c > ( 1ULL << 31 ) )
				c = 1ULL << 31;
			return std::uint32_t( c );
		};
		newVertCap = size( liveV, kFloorVerts );
		newIdxCap = size( liveI, kFloorIdx );
	}
	//! the live part fell under half of an arena above the floor: re-make it smaller (never the red)
	bool shrinkWanted() const
	{
		if ( grow )
			return false;
		return ( vertCap > kFloorVerts && liveVerts() * 2 < vertCap ) || ( idxCap > kFloorIdx && liveIdx() * 2 < idxCap );
	}
};

//! a packed rebuild: each live span's new place, in the order given (the copy moves old -> new range by range)
inline void pack( std::vector<Span> & spans, Arena & a, std::uint32_t newVertCap, std::uint32_t newIdxCap )
{
	std::uint32_t v = 0, i = 0;
	for ( Span & s : spans ) {
		s.firstVert = v;
		s.firstIndex = i;
		v += s.numVerts;
		i += s.numIndices;
	}
	a.vertCap = newVertCap;
	a.idxCap = newIdxCap;
	a.vertUsed = v;
	a.idxUsed = i;
	a.vertGarbage = a.idxGarbage = 0;
	// lane BINDREOPEN1: the old buffers go whole (GL keeps them until the draws that read them are done)
	a.fv.clear();
	a.fi.clear();
	a.graves.clear();
	a.graveVerts = a.graveIdx = 0;
}

/*! the draw arguments of a span: indices relative to the shape are drawn from `firstIndex` (a byte offset into the
 *  arena's element buffer) with the span's first vertex as the base. `redPrev` (WW_BINDLESS_RED=pooloff1) is the red:
 *  the span drawn is the one allocated before it, one draw off */
inline void drawArgs( const Span & s, const Span * redPrev, std::uintptr_t & indexByteOffset, std::int32_t & baseVertex )
{
	const Span & d = redPrev ? *redPrev : s;
	indexByteOffset = std::uintptr_t( d.firstIndex ) * 2u;
	baseVertex = std::int32_t( d.firstVert );
}
}

#endif
