#ifndef LARGEREF_H
#define LARGEREF_H

/* lane GEOLOD1 (LARGEREF1): the worldspace's large references (WRLD RNAM), std only so tests/features/far-lod/largeref builds it with g++.
 * One RNAM subrecord = int16 gridY, int16 gridX, uint32 count, then count x { uint32 ref, int16 cellY, int16 cellX }.
 * Measured on Fallout4.esm Commonwealth: read Y,X, 63954 of 63954 rows put the ref in the cell its position says; read X,Y
 * only 911 do (the red). The rule: a large ref draws full while any cell whose RNAM lists it is loaded. */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
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

/* lane LARGEREF4 (LARGEREF-FLAT-MIXED, LARGEREF-BUDGET): the far field's kind cell by cell, as it was built. The FO4CS
 * type lets the game's .bto fill the chunks our .lodl lacks, and an object ring the budget refuses leaves its cells
 * with no far objects at all, so one kind per mode is wrong somewhere. Rectangles in inclusive cells; the first that
 * holds a cell wins; a cell no rectangle holds is None. cellFarLodAppend fills it from inside out: the hole round the
 * open, then per ring ours (its object ring's kind), the game's chunks that loaded a .bto (Flat), the ring's footprint
 * (None: what is left of it drew no far objects). */
class FarMap
{
public:
	struct Rect { int x0, y0, x1, y1; Far kind; };

	void add( int x0, int y0, int x1, int y1, Far kind )
	{
		if ( x1 >= x0 && y1 >= y0 )
			rects_.push_back( Rect{ x0, y0, x1, y1, kind } );
	}
	Far at( int x, int y ) const
	{
		for ( const Rect & r : rects_ )
			if ( x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1 )
				return r.kind;
		return Far::None;
	}
	bool empty() const { return rects_.empty(); }
	size_t size() const { return rects_.size(); }
	//! Rectangles of a kind (the census line)
	int count( Far k ) const
	{
		int n = 0;
		for ( const Rect & r : rects_ )
			n += r.kind == k ? 1 : 0;
		return n;
	}

private:
	std::vector<Rect> rects_;
};

/* lane LARGEREF4 (LARGEREF-MAP-LIVE): a live far rebuild cuts the streamed cells and starts the stream again; the new
 * stream reads the far build's map as it is now. now: that map (null: the far LOD went, or none was built); held: the
 * one the stream read before. A far LOD removed live leaves no far copy: the open's kind gives way to None. Red stale
 * (WW_CELL_LARGEREFS_RED=stalemap): the held map and the open's kind stay. */
inline const FarMap * streamMap( const FarMap * now, const FarMap * held, bool stale ) { return stale && held ? held : now; }
inline Far liveFar( bool farBuilt, Far before, bool stale ) { return farBuilt || stale ? before : Far::None; }

/* lane LARGEREF4 (LARGEREF-FIXEDOPEN-MIXED): a fixed open (no stream) places its large refs by the mode before the far
 * build runs; once the build's map exists a placed ref stays full only where the map at its own cell wants it full
 * (cellview.cpp hides the rest). Red perMode (WW_CELL_LARGEREFS_RED=fixedpermode): the mode alone decides.
 * lane LARGEREF5: no map (the far build made none) falls back to the open's kind `mode`, not to full. */
inline bool fixedFull( bool placed, const FarMap * map, int ownX, int ownY, bool flatDouble, bool perMode, Far mode )
{
	return placed && ( perMode || ( map ? fullOutside( map->at( ownX, ownY ), flatDouble ) : fullOutside( mode, flatDouble ) ) );
}

/* lane LARGEREF5 (LARGEREF-FIXEDOPEN-FLATMODE): what a fixed open places before the far build. In flat mode it placed
 * none, so a ref whose own cell the far map calls None (a ring the budget refused, a chunk with no .bto)
 * showed nothing outside its own cell. Now every outside ref is placed in buckets of its own and the map hides the flat
 * ones (fixedFull). byMode (the fixedpermode red, the stream's own open, red WW_CELL_LARGEREFS_RED=fixedflatnone): the
 * mode alone, as before. */
inline bool fixedPlaced( Far mode, bool byMode, bool flatDouble ) { return byMode ? fullOutside( mode, flatDouble ) : true; }

/* lane LARGEREF6 (LARGEREF-FLATMASK-FIX): the flat cell mask (cellfarhide.cpp) degenerates far triangles centred in a
 * loaded cell, so a wide ref standing one cell out loses the part of its far copy that reaches in (median 18 %, up to
 * 97 %, notes/largeref4/tmp/flatmask_3C.txt). Three answers, WW_CELL_LARGEREFS_FLATMASK: accept (the cut stays),
 * keepbox (the mask keeps the triangles inside the far copy's footprint), promote[:share] (a listed ref whose
 * footprint share over the loaded cells reaches `share` joins the layer: drawn full, its far copy's rest beside it).
 * Footprint: the REFR's position + scale * OBND turned by its Z rotation (the game's z turns clockwise seen from above,
 * as notes/largeref4/tools/flatmask_census.py measured it): a turned rectangle in world units, with its bound. */
enum class FlatMask { Accept = 0, KeepBox = 1, Promote = 2 };
struct Box
{
	float x0 = 1, y0 = 1, x1 = 0, y1 = 0;   //!< the bound; empty (x0 > x1): no footprint known
	float cx = 0, cy = 0, hx = 0, hy = 0;   //!< the rectangle's centre and half sizes
	float c = 1, s = 0;                     //!< cos / sin of the turn
	bool valid() const { return x1 >= x0 && y1 >= y0 && hx > 0 && hy > 0; }
	//! world -> the rectangle's own axes
	void local( float x, float y, float & lx, float & ly ) const
	{
		const float dx = x - cx, dy = y - cy;
		lx = dx * c - dy * s;
		ly = dx * s + dy * c;
	}
	//! the rectangle's axes -> world
	void world( float lx, float ly, float & x, float & y ) const
	{
		x = cx + lx * c + ly * s;
		y = cy - lx * s + ly * c;
	}
	bool holds( float x, float y ) const
	{
		if ( !valid() || x < x0 || x > x1 || y < y0 || y > y1 )
			return false;
		float lx = 0, ly = 0;
		local( x, y, lx, ly );
		return std::fabs( lx ) <= hx && std::fabs( ly ) <= hy;
	}
	bool operator==( const Box & o ) const
	{
		return cx == o.cx && cy == o.cy && hx == o.hx && hy == o.hy && c == o.c && s == o.s && x0 == o.x0 && y0 == o.y0;
	}
};
inline Box footprint( const float pos[3], float rz, float scale, const float lo[3], const float hi[3] )
{
	Box b;
	b.c = std::cos( rz );
	b.s = std::sin( rz );
	b.hx = ( hi[0] - lo[0] ) * 0.5f * scale;
	b.hy = ( hi[1] - lo[1] ) * 0.5f * scale;
	if ( b.hx <= 0 || b.hy <= 0 )
		return Box();
	const float mx = ( lo[0] + hi[0] ) * 0.5f * scale, my = ( lo[1] + hi[1] ) * 0.5f * scale;
	b.cx = pos[0] + mx * b.c + my * b.s;
	b.cy = pos[1] - mx * b.s + my * b.c;
	const float ex = std::fabs( b.hx * b.c ) + std::fabs( b.hy * b.s ), ey = std::fabs( b.hx * b.s ) + std::fabs( b.hy * b.c );
	b.x0 = b.cx - ex;
	b.x1 = b.cx + ex;
	b.y0 = b.cy - ey;
	b.y1 = b.cy + ey;
	return b;
}
//! The point lies in one of the boxes (the mask keeps that triangle)
inline bool inBox( float x, float y, const std::vector<Box> & boxes )
{
	for ( const Box & b : boxes )
		if ( b.holds( x, y ) )
			return true;
	return false;
}
//! The mode an env value names; `share` = promote's threshold (0.25, or `promote:<share>`)
inline FlatMask flatMaskOf( const std::string & v, float & share )
{
	share = 0.25f;
	if ( v == "keepbox" )
		return FlatMask::KeepBox;
	if ( v.compare( 0, 7, "promote" ) == 0 ) {
		if ( v.size() > 8 && v[7] == ':' )
			share = std::max( 0.0f, std::min( 1.0f, float( std::atof( v.c_str() + 8 ) ) ) );
		return FlatMask::Promote;
	}
	return FlatMask::Accept;
}

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
	/* lane LARGEREF4: the far kind per ref, at its own cell (where its far copy stands); replaces setFullOutside.
	 * Red WW_CELL_LARGEREFS_RED=permode keeps the one kind per mode (setFullOutside, no map). */
	void setFarMap( const FarMap & m, bool flatDouble )
	{
		map_ = m;
		mapOn_ = true;
		flatDouble_ = flatDouble;
	}
	bool hasFarMap() const { return mapOn_; }
	//! The far kind at the ref's own cell; `fallback` without a map (or an unlisted ref)
	Far farAt( uint32_t ref, Far fallback ) const
	{
		int x = 0, y = 0;
		if ( !mapOn_ || !ownCell( ref, x, y ) )
			return fallback;
		return map_.at( x, y );
	}

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
		auto bc = boxCells_.find( k );   // lane LARGEREF6: the refs whose footprint reaches this cell (their cut share moved)
		if ( bc != boxCells_.end() )
			touched.insert( touched.end(), bc->second.begin(), bc->second.end() );
		bool changed = false;
		for ( uint32_t r : touched )
			changed = settle( r ) || changed;
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

	/* lane LARGEREF6 (LARGEREF-FLATMASK-FIX): the flat mask's answer. A candidate is a listed ref drawn by its far copy
	 * alone (a flat far kind at its own cell, that cell not loaded); its footprint comes from the caller (pending ->
	 * setFootprint, the REFR and OBND read lazily), then reeval() decides. */
	void setFlatMask( FlatMask m, float share )
	{
		mask_ = m;
		share_ = share;
	}
	FlatMask flatMask() const { return mask_; }
	float promoteShare() const { return share_; }
	//! The ref's far copy is a flat field's (masked where a loaded cell lies, never hidden by ref)
	bool flatFar( uint32_t r ) const { return mapOn_ ? farAt( r, Far::None ) == Far::Flat : !full_; }
	//! A listed ref seen by its flat far copy alone: the mask cuts what reaches into the loaded cells
	bool candidate( uint32_t r ) const { return flatFar( r ) && listers( r ) > 0 && !ownLoaded( r ); }
	//! Candidates whose footprint was never given (empty once every one was); cleared by setFootprint
	const std::set<uint32_t> & pending() const { return pending_; }
	//! The footprint (an empty Box when the REFR has none): the ref leaves pending
	void setFootprint( uint32_t r, const Box & b )
	{
		pending_.erase( r );
		if ( boxes_.count( r ) )
			return;
		boxes_[r] = b;
		dirty_.insert( r );
		if ( !b.valid() )
			return;
		for ( int y = cellOf( b.y0 ); y <= cellOf( b.y1 ); y++ )
			for ( int x = cellOf( b.x0 ); x <= cellOf( b.x1 ); x++ )
				boxCells_[key( x, y )].push_back( r );
	}
	bool hasFootprint( uint32_t r ) const { return boxes_.count( r ) > 0; }
	//! Share of the footprint's area over the loaded cells (0 without a footprint)
	float cutShare( uint32_t r ) const
	{
		auto it = boxes_.find( r );
		return it == boxes_.end() ? 0.0f : cutShare( it->second );
	}
	//! Exact: the turned rectangle's area inside the loaded cells (each cell's square clips it) over its own area
	float cutShare( const Box & b ) const
	{
		if ( !b.valid() || loaded_.empty() )
			return 0.0f;
		float in = 0.0f;
		for ( int y = cellOf( b.y0 ); y <= cellOf( b.y1 ); y++ )
			for ( int x = cellOf( b.x0 ); x <= cellOf( b.x1 ); x++ )
				if ( loaded_.count( key( x, y ) ) )
					in += clipArea( b, x, y );
		return std::min( 1.0f, in / ( 4.0f * b.hx * b.hy ) );
	}
	//! The turned rectangle's area inside cell (x, y): its corners clipped to the square, then the shoelace sum
	static float clipArea( const Box & b, int x, int y )
	{
		float ax[10], ay[10], bx[10], by[10];
		int n = 4;
		b.world( -b.hx, -b.hy, ax[0], ay[0] );
		b.world( b.hx, -b.hy, ax[1], ay[1] );
		b.world( b.hx, b.hy, ax[2], ay[2] );
		b.world( -b.hx, b.hy, ax[3], ay[3] );
		n = clipHalf( ax, ay, n, bx, by, 0, x * 4096.0f, true );
		n = clipHalf( bx, by, n, ax, ay, 0, ( x + 1 ) * 4096.0f, false );
		n = clipHalf( ax, ay, n, bx, by, 1, y * 4096.0f, true );
		n = clipHalf( bx, by, n, ax, ay, 1, ( y + 1 ) * 4096.0f, false );
		if ( n < 3 )
			return 0.0f;
		float a = 0.0f;
		for ( int i = 0; i < n; i++ ) {
			const int j = ( i + 1 ) % n;
			a += ax[i] * ay[j] - ax[j] * ay[i];
		}
		return std::fabs( a ) * 0.5f;
	}
	//! One Sutherland-Hodgman step: the polygon's part on the kept side of axis = v (0: x, 1: y)
	static int clipHalf( const float * ix, const float * iy, int n, float * ox, float * oy, int axis, float v, bool keepGreater )
	{
		int m = 0;
		for ( int i = 0; i < n; i++ ) {
			const int j = ( i + 1 ) % n;
			const float a = axis ? iy[i] : ix[i], bq = axis ? iy[j] : ix[j];
			const bool ina = keepGreater ? a >= v : a <= v, inb = keepGreater ? bq >= v : bq <= v;
			if ( ina ) {
				ox[m] = ix[i];
				oy[m] = iy[i];
				m++;
			}
			if ( ina != inb ) {
				const float t = ( v - a ) / ( bq - a );
				ox[m] = ix[i] + ( ix[j] - ix[i] ) * t;
				oy[m] = iy[i] + ( iy[j] - iy[i] ) * t;
				m++;
			}
		}
		return m;
	}
	//! promote: the candidate's cut share reaches the threshold, so the layer draws it full
	bool promoted( uint32_t r ) const { return mask_ == FlatMask::Promote && candidate( r ) && cutShare( r ) >= share_; }
	//! Refs the layer settles once their footprints arrived (setFootprint); true when the layer changed
	bool reeval()
	{
		bool changed = false;
		for ( uint32_t r : dirty_ )
			changed = settle( r ) || changed;
		dirty_.clear();
		return changed;
	}
	//! keepbox: the footprints the mask keeps now (candidates whose box reaches a loaded cell), sorted by ref
	std::vector<Box> keepBoxes() const
	{
		std::vector<Box> out;
		if ( mask_ != FlatMask::KeepBox )
			return out;
		for ( const auto & kv : boxes_ )
			if ( kv.second.valid() && candidate( kv.first ) && cutShare( kv.second ) > 0.0f )
				out.push_back( kv.second );
		return out;
	}
	//! Candidates the mask cuts now, and of them the promoted ones (the stream's line)
	void maskCounts( int & cut, int & promotedN ) const
	{
		cut = promotedN = 0;
		for ( const auto & kv : boxes_ ) {
			if ( !kv.second.valid() || !candidate( kv.first ) || cutShare( kv.second ) <= 0.0f )
				continue;
			cut++;
			promotedN += promoted( kv.first ) ? 1 : 0;
		}
	}

private:
	static int64_t key( int x, int y ) { return int64_t( ( uint64_t( uint32_t( x ) ) << 32 ) | uint64_t( uint32_t( y ) ) ); }
	static int cellOf( float w ) { return int( std::floor( w / 4096.0f ) ); }
	bool fullFor( uint32_t r ) const
	{
		return mapOn_ ? LargeRef::fullOutside( farAt( r, Far::None ), flatDouble_ ) : full_;	// lane LARGEREF4
	}
	// lane LARGEREF6: a promoted candidate joins the layer too
	bool inLayer( uint32_t r ) const { return ( fullFor( r ) || promoted( r ) ) && listers( r ) > 0 && ( red_ || !ownLoaded( r ) ); }
	//! The layer takes or lets go of the ref as inLayer says; a candidate without a footprint joins pending
	bool settle( uint32_t r )
	{
		if ( mask_ != FlatMask::Accept && candidate( r ) && !boxes_.count( r ) )
			pending_.insert( r );
		else
			pending_.erase( r );
		const bool want = inLayer( r );
		if ( want == ( layer_.count( r ) > 0 ) )
			return false;
		if ( want )
			layer_.insert( r );
		else
			layer_.erase( r );
		return true;
	}

	bool red_ = false;
	bool full_ = true;	// lane LARGEREF3
	FarMap map_;	// lane LARGEREF4
	bool mapOn_ = false, flatDouble_ = false;
	std::map<int64_t, std::vector<uint32_t>> lists_;      //!< grid cell -> refs it lists
	std::map<int64_t, std::vector<uint32_t>> standing_;   //!< cell -> refs standing in it
	std::map<uint32_t, int64_t> own_;                     //!< ref -> its own cell
	std::map<uint32_t, int> count_;                       //!< ref -> loaded listing cells
	std::set<int64_t> loaded_;
	std::set<uint32_t> layer_;
	FlatMask mask_ = FlatMask::Accept;	// lane LARGEREF6
	float share_ = 0.25f;
	std::map<uint32_t, Box> boxes_;                       //!< ref -> its footprint (empty: none known)
	std::map<int64_t, std::vector<uint32_t>> boxCells_;   //!< cell -> refs whose footprint reaches it
	std::set<uint32_t> pending_, dirty_;
};

}

#endif
