#ifndef TEXRECYCLE_H
#define TEXRECYCLE_H

/* Lane PERFFIX5 (2026-11-02): THE TEXTURE STORAGE RECYCLER, A REFUTED IDEA KEPT AS A MODEL (not wired into NifSkope).
 * In texrecycle_gl (real NVIDIA driver, 21-centre walk at 100 %) it made private WORSE: timer x1.025, recycle x1.133;
 * the driver already re-uses freed storage, and the list's held bytes add on top. Do not ship it.
 *
 * The pool (texpool.h) still freed and re-read ~2700 textures on the c3 walk (its working set ~6.9 GB is above the
 * Ultra budget), and every glDeleteTextures + glTexStorage2D of a new size left NVIDIA's driver pool at a higher mark:
 * private x1.076 over 20 crossings with the resident textures back at their start figure (TOOLS1101 1102_0003).
 * Now an evicted texture's name and its immutable storage (target, internal format, width, height, levels) go to a
 * small free list; the next load of the same shape re-fills that storage (glCompressedTexSubImage2D only), so the
 * driver allocates nothing new. The list is capped in bytes; past the cap the oldest go to glDeleteTextures.
 * The model's "timer" mode (every evicted texture deleted) is the base it lost to. */

#include <cstdint>
#include <vector>

namespace WwTexRecycle
{

struct Key
{
	std::uint32_t target = 0, format = 0;
	std::int32_t w = 0, h = 0, levels = 0;
	bool operator==( const Key & o ) const
	{
		return target == o.target && format == o.format && w == o.w && h == o.h && levels == o.levels;
	}
};

class List
{
public:
	struct Item
	{
		Key key;
		std::uint32_t id = 0;
		std::int64_t bytes = 0;
	};

	//! Puts a freed texture on the list. Returns the names to delete now (the oldest, while the list is over `cap`).
	std::vector<std::uint32_t> put( const Key & k, std::uint32_t id, std::int64_t bytes, std::int64_t cap )
	{
		std::vector<std::uint32_t> out;
		if ( !id )
			return out;
		if ( bytes > cap ) {	// never kept: bigger than the whole list
			out.push_back( id );
			return out;
		}
		items.push_back( { k, id, bytes } );
		held += bytes;
		std::size_t drop = 0;
		while ( held > cap && drop < items.size() ) {
			out.push_back( items[drop].id );
			held -= items[drop].bytes;
			drop++;
		}
		if ( drop )
			items.erase( items.begin(), items.begin() + std::ptrdiff_t( drop ) );
		return out;
	}

	//! The newest freed name with this storage shape, taken off the list; 0 if none.
	std::uint32_t take( const Key & k )
	{
		for ( std::size_t i = items.size(); i-- > 0; ) {
			if ( items[i].key == k ) {
				const std::uint32_t id = items[i].id;
				held -= items[i].bytes;
				items.erase( items.begin() + std::ptrdiff_t( i ) );
				taken++;
				return id;
			}
		}
		return 0;
	}

	//! Every name still held (to delete), the list emptied.
	std::vector<std::uint32_t> clear()
	{
		std::vector<std::uint32_t> out;
		for ( const Item & x : items )
			out.push_back( x.id );
		items.clear();
		held = 0;
		return out;
	}

	std::int64_t heldBytes() const { return held; }
	std::size_t size() const { return items.size(); }
	std::int64_t takenCount() const { return taken; }

private:
	std::vector<Item> items;	// oldest first
	std::int64_t held = 0, taken = 0;
};

//! The list's cap: a quarter of the pool budget, at least 256 MB (WW_CELL_STREAM_TEXRECYCLE=<MB> overrides; 0 = off).
inline std::int64_t capBytes( std::int64_t poolBudget, int envMb )
{
	if ( envMb >= 0 )
		return std::int64_t( envMb ) << 20;
	const std::int64_t q = poolBudget / 4;
	return q > ( std::int64_t( 256 ) << 20 ) ? q : ( std::int64_t( 256 ) << 20 );
}

} // namespace WwTexRecycle

#endif // TEXRECYCLE_H
