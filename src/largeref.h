#ifndef LARGEREF_H
#define LARGEREF_H

/* lane GEOLOD1 (LARGEREF1): the worldspace's large references (WRLD RNAM), std only so tests/largeref builds it with g++.
 * One RNAM subrecord = int16 gridY, int16 gridX, uint32 count, then count x { uint32 ref, int16 cellY, int16 cellX }.
 * Measured on Fallout4.esm Commonwealth: read Y,X, 63954 of 63954 rows put the ref in the cell its position says; read X,Y
 * only 911 do (the red). The rule: a large ref draws full while any cell whose RNAM lists it is loaded. */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace LargeRef
{

struct Row
{
	int gx = 0, gy = 0;   //!< the grid cell whose list holds the ref
	uint32_t ref = 0;     //!< file-local form id (the caller maps it)
	int rx = 0, ry = 0;   //!< the cell the ref stands in
};

inline int16_t i16( const unsigned char * p )
{
	int16_t v;
	std::memcpy( &v, p, 2 );
	return v;
}

inline uint32_t u32( const unsigned char * p )
{
	uint32_t v;
	std::memcpy( &v, p, 4 );
	return v;
}

//! Append one RNAM subrecord's rows; yx = the measured order (false = the red X,Y reading). Returns rows read, -1 if short.
inline int decode( const unsigned char * d, size_t n, bool yx, std::vector<Row> & out )
{
	if ( n < 8 )
		return -1;
	const int a = i16( d ), b = i16( d + 2 );
	const uint32_t cnt = u32( d + 4 );
	if ( size_t( cnt ) * 8 + 8 > n )
		return -1;
	for ( uint32_t k = 0; k < cnt; k++ ) {
		const unsigned char * p = d + 8 + 8 * size_t( k );
		Row r;
		r.ref = u32( p );
		const int c = i16( p + 4 ), e = i16( p + 6 );
		r.gx = yx ? b : a;
		r.gy = yx ? a : b;
		r.rx = yx ? e : c;
		r.ry = yx ? c : e;
		out.push_back( r );
	}
	return int( cnt );
}

//! The refs that cells x0..x1, y0..y1 list and that stand outside that rectangle: one row per ref (first seen).
inline std::vector<Row> outside( const std::vector<Row> & rows, int x0, int y0, int x1, int y1 )
{
	std::vector<Row> out;
	std::set<uint32_t> seen;
	auto in = [&]( int x, int y ) { return x >= x0 && x <= x1 && y >= y0 && y <= y1; };
	for ( const Row & r : rows ) {
		if ( !in( r.gx, r.gy ) || in( r.rx, r.ry ) || !seen.insert( r.ref ).second )
			continue;
		out.push_back( r );
	}
	return out;
}

/* lane LARGEREF3 (LARGEREF-FLAT): what the far object field can do with one large ref's far copy. ByRef: our instanced
 * rings hide its far rows (CellFarHide::setExtraRefs). Flat: flattened rings or the game's .bto chunks mask whole loaded
 * cells only, so the far copy stays while its own cell is not loaded. None: no far objects at all. */
enum class Far { None = 0, ByRef = 1, Flat = 2 };

//! The far field's kind from its inputs (cellFarLodObjectsFar): far LOD wanted, NEARONLY, ours (FO4CS type + .lodl),
//! red noobjects (ours not built, the .bto hidden), our rings instanced (not WW_CELL_FARLOD_RED=flat/perbase, side mesh on)
inline Far farOf( bool farWanted, bool nearOnly, bool ours, bool noObjects, bool instanced )
{
	if ( !farWanted || nearOnly || ( ours && noObjects ) )
		return Far::None;
	return ours && instanced ? Far::ByRef : Far::Flat;
}

//! Manager ruling (a): over a flat far field a large ref is NOT drawn full outside its own cell (its far copy only, the
//! vanilla look). `flatDouble` = red WW_CELL_LARGEREFS_RED=flatdouble: drawn full there too, beside its far copy.
inline bool fullOutside( Far f, bool flatDouble ) { return f != Far::Flat || flatDouble; }

/* lane LARGEREF2 (LARGEREF-STREAM): per-document ownership counts for the streamed view. A large ref is drawn by exactly
 * one owner: its own cell while that cell is loaded (the cell's own refs place it), else the large-ref layer while any
 * loaded cell lists it (counts[ref] = loaded listing cells). So a ref is never drawn by two listing cells, nor by a
 * listing cell and its own cell. `red` (WW_CELL_LARGEREFS_RED=streamdouble) keeps it in the layer while its own cell is
 * loaded: the double draw comes back. */
class Owners
{
public:
	explicit Owners( bool red = false ) : red_( red ) {}

	//! lane LARGEREF3: false (a flat far field, fullOutside) = the layer draws nothing; call before the first cell()
	void setFullOutside( bool on ) { full_ = on; }
	bool fullOutside() const { return full_; }

	void setRows( const std::vector<Row> & rows )
	{
		lists_.clear();
		own_.clear();
		standing_.clear();
		count_.clear();
		loaded_.clear();
		layer_.clear();
		for ( const Row & r : rows ) {
			std::vector<uint32_t> & l = lists_[key( r.gx, r.gy )];
			if ( std::find( l.begin(), l.end(), r.ref ) == l.end() )
				l.push_back( r.ref );
			if ( own_.emplace( r.ref, key( r.rx, r.ry ) ).second )
				standing_[key( r.rx, r.ry )].push_back( r.ref );
		}
	}

	//! A cell joined (on) or left the loaded set; a repeat call is a no-op. Returns true when the layer changed.
	bool cell( int x, int y, bool on )
	{
		const int64_t k = key( x, y );
		if ( on == ( loaded_.count( k ) > 0 ) )
			return false;
		if ( on )
			loaded_.insert( k );
		else
			loaded_.erase( k );
		std::vector<uint32_t> touched;
		auto it = lists_.find( k );
		if ( it != lists_.end() )
			for ( uint32_t r : it->second ) {
				int & c = count_[r];
				c += on ? 1 : -1;
				if ( c <= 0 )
					count_.erase( r );
				touched.push_back( r );
			}
		auto st = standing_.find( k );   // the refs standing in this cell change owner
		if ( st != standing_.end() )
			touched.insert( touched.end(), st->second.begin(), st->second.end() );
		bool changed = false;
		for ( uint32_t r : touched ) {
			const bool want = inLayer( r );
			if ( want == ( layer_.count( r ) > 0 ) )
				continue;
			if ( want )
				layer_.insert( r );
			else
				layer_.erase( r );
			changed = true;
		}
		return changed;
	}

	//! The refs the layer draws now (sorted)
	const std::set<uint32_t> & layer() const { return layer_; }
	//! Loaded cells listing the ref
	int listers( uint32_t ref ) const
	{
		auto it = count_.find( ref );
		return it == count_.end() ? 0 : it->second;
	}
	//! The ref's own cell is loaded (it places the ref itself)
	bool ownLoaded( uint32_t ref ) const
	{
		auto it = own_.find( ref );
		return it != own_.end() && loaded_.count( it->second ) > 0;
	}
	//! Full copies drawn of a large ref: its own cell's (when loaded) + the layer's
	int copies( uint32_t ref ) const { return ( ownLoaded( ref ) ? 1 : 0 ) + ( layer_.count( ref ) ? 1 : 0 ); }
	//! The ref should be drawn full: a loaded cell lists it, or it stands in a loaded cell
	bool wanted( uint32_t ref ) const { return listers( ref ) > 0 || ownLoaded( ref ); }
	//! lane LARGEREF3: its far copy is drawn (ByRef: hidden while drawn full; Flat: a loaded own cell masks it)
	int farCopy( uint32_t ref, Far f ) const
	{
		if ( f == Far::None || ownLoaded( ref ) )
			return 0;
		return f == Far::ByRef && layer_.count( ref ) ? 0 : 1;
	}
	//! lane LARGEREF3: every copy on screen, full + far: 1 for every ref while a far field exists, never 2
	int seen( uint32_t ref, Far f ) const { return copies( ref ) + farCopy( ref, f ); }
	//! The cell the ref stands in (false: no RNAM row names it)
	bool ownCell( uint32_t ref, int & x, int & y ) const
	{
		auto it = own_.find( ref );
		if ( it == own_.end() )
			return false;
		x = int( int32_t( uint32_t( uint64_t( it->second ) >> 32 ) ) );
		y = int( int32_t( uint32_t( uint64_t( it->second ) ) ) );
		return true;
	}
	size_t refs() const { return own_.size(); }

private:
	static int64_t key( int x, int y ) { return int64_t( ( uint64_t( uint32_t( x ) ) << 32 ) | uint64_t( uint32_t( y ) ) ); }
	bool inLayer( uint32_t r ) const { return full_ && listers( r ) > 0 && ( red_ || !ownLoaded( r ) ); }

	bool red_ = false;
	bool full_ = true;	// lane LARGEREF3
	std::map<int64_t, std::vector<uint32_t>> lists_;      //!< grid cell -> refs it lists
	std::map<int64_t, std::vector<uint32_t>> standing_;   //!< cell -> refs standing in it
	std::map<uint32_t, int64_t> own_;                     //!< ref -> its own cell
	std::map<uint32_t, int> count_;                       //!< ref -> loaded listing cells
	std::set<int64_t> loaded_;
	std::set<uint32_t> layer_;
};

}

#endif
