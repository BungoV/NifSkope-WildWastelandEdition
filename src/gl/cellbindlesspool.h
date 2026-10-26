#ifndef CELLBINDLESSPOOL_H
#define CELLBINDLESSPOOL_H

/* lane BINDLESS2: the geometry pool's bookkeeping, apart from GL so tests/bindlesspool can run it alone.
 * One arena per vertex layout: vertices and 16-bit indices are appended; a shape draws its span with a base vertex
 * (its indices stay the shape's own) and an index byte offset. A full arena is rebuilt packed (garbage dropped) at
 * twice the size when more than half of it is still live. */

#include <cstdint>
#include <vector>

namespace WwBlPool
{
struct Span
{
	std::uint32_t firstVert = 0, numVerts = 0;
	std::uint32_t firstIndex = 0, numIndices = 0;
};

struct Arena
{
	std::uint32_t vertCap = 0, idxCap = 0;	// in vertices / indices
	std::uint32_t vertUsed = 0, idxUsed = 0;	// the append points
	std::uint32_t vertGarbage = 0, idxGarbage = 0;

	//! a span at the append point, or false when it does not fit (the caller grows or packs, then asks again)
	bool alloc( std::uint32_t verts, std::uint32_t idx, Span & s )
	{
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
	void release( const Span & s )
	{
		vertGarbage += s.numVerts;
		idxGarbage += s.numIndices;
	}
	//! the capacities a rebuild takes so that `verts`/`idx` more fit: packed, doubled until it fits (64k/192k floor)
	void plan( std::uint32_t verts, std::uint32_t idx, std::uint32_t & newVertCap, std::uint32_t & newIdxCap ) const
	{
		const std::uint64_t liveV = std::uint64_t( vertUsed - vertGarbage ) + verts;
		const std::uint64_t liveI = std::uint64_t( idxUsed - idxGarbage ) + idx;
		std::uint64_t v = vertCap ? vertCap : 65536, i = idxCap ? idxCap : 196608;
		while ( v < liveV * 2 && v < ( 1ULL << 31 ) )
			v *= 2;
		while ( i < liveI * 2 && i < ( 1ULL << 31 ) )
			i *= 2;
		newVertCap = std::uint32_t( v );
		newIdxCap = std::uint32_t( i );
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
