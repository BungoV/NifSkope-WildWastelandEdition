// lane GEOLOD1 (LARGEREF1): the RNAM decode + the large-ref rule of src/largeref.h.
// g++ -std=c++17 -I<tree>/src tests/largeref/largeref_test.cpp; -DLARGEREF_RED reads X,Y (the red) and must fail.
#include "largeref.h"
#include "gameinikey.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

#ifdef LARGEREF_RED
static const bool YX = false;
#else
static const bool YX = true;
#endif
#ifdef LARGEREF_STREAM_RED	// lane LARGEREF2: the layer keeps a ref its own loaded cell draws (the double draw)
static const bool STREAMRED = true;
#else
static const bool STREAMRED = false;
#endif
#ifdef LARGEREF_FLAT_RED	// lane LARGEREF3: over a flat far field a large ref is drawn full beside its far copy
static const bool FLATRED = true;
#else
static const bool FLATRED = false;
#endif
#ifdef LARGEREF_PERMODE_RED	// lane LARGEREF4: one far kind per mode (the FO4CS type's ByRef), no per-cell map
static const bool PERMODERED = true;
#else
static const bool PERMODERED = false;
#endif
#ifdef LARGEREF_STALEMAP_RED	// lane LARGEREF4 (MAP-LIVE): a live far rebuild keeps the map read before it
static const bool STALERED = true;
#else
static const bool STALERED = false;
#endif
#ifdef LARGEREF_FIXEDOPEN_RED	// lane LARGEREF4 (FIXEDOPEN-MIXED): the fixed open keeps the mode's answer, no map
static const bool FIXEDRED = true;
#else
static const bool FIXEDRED = false;
#endif
#ifdef LARGEREF_FIXEDFLAT_RED	// lane LARGEREF5 (FIXEDOPEN-FLATMODE): a flat-mode fixed open places no large ref full
static const bool FLATNONERED = true;
#else
static const bool FLATNONERED = false;
#endif

/* lane LARGEREF5: a flat far field round a fixed open at cx,cy as cellFarLodAppend maps it (a fixed open's hole is the
 * block itself): the hole None (+-2), ring 0 Flat to +-8 except one 4x4 chunk east (+4..+7, -1..+2) with no .bto (its
 * footprint None), ring 1 refused by the budget (None to +-16), the outer rings Flat to +-40, None past the reach. */
static LargeRef::FarMap flatModeMap( int cx, int cy )
{
	using LargeRef::Far;
	LargeRef::FarMap m;
	m.add( cx - 2, cy - 2, cx + 2, cy + 2, Far::None );
	m.add( cx + 4, cy - 1, cx + 7, cy + 2, Far::None );
	m.add( cx - 8, cy - 8, cx + 8, cy + 8, Far::Flat );
	m.add( cx - 16, cy - 16, cx + 16, cy + 16, Far::None );
	m.add( cx - 40, cy - 40, cx + 40, cy + 40, Far::Flat );
	return m;
}

/* lane LARGEREF4: far fields as cellFarLodAppend maps them, round an open at 0,0 (the census routes cross every band).
 * mixed: the FO4CS type, ours instanced to +-12, the game's .bto past it (WW_CELL_FARLOD_OURS, or a .lodl smaller than
 * the reach). budget: ours instanced to +-8, the next object ring refused by the budget (its cells: no far objects),
 * the outer ring's terrain not built, so the game's .bto fills it. */
static LargeRef::FarMap censusMap( int mode )
{
	using LargeRef::Far;
	LargeRef::FarMap m;
	m.add( -2, -2, 2, 2, Far::None );	// the hole round the open
	if ( mode == 1 ) {
		m.add( -12, -12, 12, 12, Far::ByRef );
		m.add( -64, -64, 64, 64, Far::Flat );
	} else {
		m.add( -8, -8, 8, 8, Far::ByRef );
		m.add( -16, -16, 16, 16, Far::None );
		m.add( -64, -64, 64, 64, Far::Flat );
	}
	return m;
}

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

static void put16( std::vector<unsigned char> & b, int v )
{
	const int16_t s = int16_t( v );
	unsigned char t[2];
	std::memcpy( t, &s, 2 );
	b.insert( b.end(), t, t + 2 );
}

static void put32( std::vector<unsigned char> & b, uint32_t v )
{
	unsigned char t[4];
	std::memcpy( t, &v, 4 );
	b.insert( b.end(), t, t + 4 );
}

// one subrecord in the file's layout: gridY, gridX, count, { ref, cellY, cellX }
static std::vector<unsigned char> rnam( int gy, int gx, const std::vector<LargeRef::Row> & refs )
{
	std::vector<unsigned char> b;
	put16( b, gy );
	put16( b, gx );
	put32( b, uint32_t( refs.size() ) );
	for ( const LargeRef::Row & r : refs ) {
		put32( b, r.ref );
		put16( b, r.ry );
		put16( b, r.rx );
	}
	return b;
}

static LargeRef::Row ref( uint32_t f, int x, int y )
{
	LargeRef::Row r;
	r.ref = f;
	r.rx = x;
	r.ry = y;
	return r;
}

/* lane LARGEREF2: the census walk over a real world's RNAM (tools rnam_dump.py: uint32 length + subrecord, repeated).
 * A 5x5 block slides along routes one cell a step, its cells attaching one at a time as the stream attaches them;
 * after every attach or leave no ref may have two copies, and after every step each wanted ref has exactly one. */
static bool readRnam( const char * path, std::vector<LargeRef::Row> & rows, int & subs, int & refused )
{
	std::FILE * f = std::fopen( path, "rb" );
	if ( !f ) {
		std::printf( "census: cannot read %s\n", path );
		return false;
	}
	subs = refused = 0;
	for ( ;; ) {
		uint32_t n = 0;
		if ( std::fread( &n, 4, 1, f ) != 1 )
			break;
		std::vector<unsigned char> b( n );
		if ( n && std::fread( b.data(), 1, n, f ) != n )
			break;
		subs++;
		refused += LargeRef::decode( b.data(), b.size(), YX, rows ) < 0 ? 1 : 0;
	}
	std::fclose( f );
	return true;
}

// the census routes (block centres): Sanctuary east, Concord to downtown, along downtown, the south coast up north
static const int routes[4][4] = { { -17, 23, 12, 23 }, { -15, 17, 2, -2 }, { -10, -2, 30, -2 }, { 25, -25, 25, 20 } };

/* mapMode 3 / 4 (lane LARGEREF4, LARGEREF-MAP-LIVE): a live far rebuild as the third route starts: 3 the budget
 * field rebuilt whole (the mixed one), 4 the far LOD removed. The stream starts again (cellStreamStart): new owners,
 * the map read now, the cells streaming in again. */
static int census( const char * path, LargeRef::Far far, int mapMode = 0 )
{
	std::vector<LargeRef::Row> rows;
	int subs = 0, refused = 0;
	if ( !readRnam( path, rows, subs, refused ) )
		return 2;
	LargeRef::Owners o( STREAMRED );
	o.setFullOutside( LargeRef::fullOutside( far, FLATRED ) );	// lane LARGEREF3: as cellstream.cpp sets it
	// lane LARGEREF4: a mapped field (mixed / budget): the truth is the map; green owners read it, the red the mode's kind
	LargeRef::FarMap truth = censusMap( mapMode == 3 ? 2 : mapMode == 4 ? 1 : mapMode );
	int switched = 0;
	if ( mapMode && !PERMODERED )
		o.setFarMap( truth, FLATRED );
	o.setRows( rows );
	auto kindOf = [&]( uint32_t r ) {
		if ( !mapMode )
			return far;
		int x = 0, y = 0;
		return o.ownCell( r, x, y ) ? truth.at( x, y ) : LargeRef::Far::None;
	};
	std::set<uint32_t> all;
	for ( const LargeRef::Row & r : rows )
		all.insert( r.ref );
	std::set<std::pair<int, int>> loaded;
	long events = 0, twice = 0, stepsBad = 0, steps = 0;
	size_t maxLayer = 0, maxWanted = 0;
	std::set<uint32_t> everLayer;
	auto watch = [&]() {
		events++;
		for ( uint32_t r : o.layer() )   // only a layer ref can be a second copy (full + full, or full + far)
			twice += o.seen( r, kindOf( r ) ) > 1 ? 1 : 0;
	};
	long farOnly = 0;   // lane LARGEREF3: step x ref seen as its far copy alone while a loaded cell lists it
	for ( int ri = 0; ri < 4; ri++ ) {
		const int * rt = routes[ri];
		if ( mapMode >= 3 && ri == 2 ) {	// lane LARGEREF4 (LARGEREF-MAP-LIVE): the live far rebuild
			const LargeRef::FarMap held = truth;
			const bool built = mapMode == 3;
			truth = built ? censusMap( 1 ) : LargeRef::FarMap();
			o = LargeRef::Owners( STREAMRED );
			o.setFullOutside( LargeRef::fullOutside( LargeRef::liveFar( built, far, STALERED ), FLATRED ) );
			const LargeRef::FarMap * use = LargeRef::streamMap( built ? &truth : nullptr, &held, STALERED );
			if ( use && !PERMODERED )
				o.setFarMap( *use, FLATRED );
			o.setRows( rows );
			loaded.clear();	// the rebuild cut the streamed cells; they stream in again
			switched++;
		}
		int x = rt[0], y = rt[1];
		for ( ;; ) {
			std::set<std::pair<int, int>> next;
			for ( int dy = -2; dy <= 2; dy++ )
				for ( int dx = -2; dx <= 2; dx++ )
					next.insert( { x + dx, y + dy } );
			for ( const auto & k : next )   // the stream attaches the new cells before the old ones leave
				if ( !loaded.count( k ) ) {
					o.cell( k.first, k.second, true );
					watch();
				}
			for ( const auto & k : loaded )
				if ( !next.count( k ) ) {
					o.cell( k.first, k.second, false );
					watch();
				}
			loaded = next;
			int bad = 0;
			size_t wantedN = 0;
			for ( uint32_t r : all ) {
				const bool w = o.wanted( r );
				wantedN += w ? 1 : 0;
				// lane LARGEREF3: with a far field every ref is on screen once (full or far); without, once while wanted
				const LargeRef::Far k = kindOf( r );	// lane LARGEREF4: per ref on a mapped field
				bad += k == LargeRef::Far::None ? o.copies( r ) != ( w ? 1 : 0 ) : o.seen( r, k ) != 1;
				farOnly += w && o.copies( r ) == 0 ? 1 : 0;
			}
			stepsBad += bad ? 1 : 0;
			steps++;
			maxLayer = std::max( maxLayer, o.layer().size() );
			maxWanted = std::max( maxWanted, wantedN );
			everLayer.insert( o.layer().begin(), o.layer().end() );
			if ( x == rt[2] && y == rt[3] )
				break;
			x += ( rt[2] > x ) - ( rt[2] < x );
			y += ( rt[3] > y ) - ( rt[3] < y );
		}
	}
	std::printf( "census: %d RNAM subrecords (%d refused), %d rows, %d refs; %ld steps, %ld cell events\n", subs, refused,
		int( rows.size() ), int( all.size() ), steps, events );
	std::printf( "census: layer up to %d units (block wanted up to %d large refs), %d refs drawn by a unit at some step\n",
		int( maxLayer ), int( maxWanted ), int( everLayer.size() ) );
	/* lane LARGEREF3: the layer's floor follows the far field: ByRef / None must draw units (a layer that never fills
	 * would pass vacuously); Flat must draw none and show the listed refs by their far copy alone */
	const bool flat = far == LargeRef::Far::Flat;
	// lane LARGEREF4: a mapped field has both: units where ours hide by ref or nothing is far, far copies over the .bto
	const bool floor = mapMode ? maxLayer > 0 && farOnly > 0 && switched == ( mapMode >= 3 ? 1 : 0 )
		: flat ? ( maxLayer == 0 && farOnly > 0 ) || FLATRED : maxLayer > 0;
	const bool pass = twice == 0 && stepsBad == 0 && refused == 0 && floor;
	std::printf( "census: far %s%s%s; listed refs shown by the far copy alone %ld step-refs\n",
		mapMode == 1 ? "mixed (ours byref +-12, the game's .bto past it)"
			: mapMode == 2 ? "budget (ours byref +-8, refused to +-16, the game's .bto past it)"
			: mapMode == 3 ? "budget, rebuilt live to mixed before route 3"
			: mapMode == 4 ? "mixed, the far LOD removed live before route 3"
			: far == LargeRef::Far::None ? "none" : flat ? "flat" : "byref",
		mapMode && PERMODERED ? " (RED permode: byref everywhere)" : "",
		mapMode >= 3 && STALERED ? " (RED stalemap: the map and kind read before the rebuild)" : "", farOnly );
	std::printf( "census: refs with two copies after a cell event %ld; steps with a wrong copy count %ld -- %s\n", twice,
		stepsBad, pass ? "PASS" : "FAIL" );
	return pass ? 0 : 1;
}

/* lane LARGEREF4 (LARGEREF-FIXEDOPEN-MIXED): a fixed open (no stream) at every step of the routes. The 5x5 block's
 * cells draw their own refs; the large refs they list standing outside are placed full by the mode (the FO4CS type:
 * ByRef), then the far build's map hides those whose own cell it calls flat (LargeRef::fixedFull). Each must be on
 * screen once: full with its far rows hidden (ByRef), its far copy alone (Flat), full alone (None). */
static int fixedCensus( const char * path, int mapMode )
{
	std::vector<LargeRef::Row> rows;
	int subs = 0, refused = 0;
	if ( !readRnam( path, rows, subs, refused ) )
		return 2;
	using LargeRef::Far;
	// lane LARGEREF5: mapMode 3 = flat mode (the vanilla type), its map round each step's open (flatModeMap)
	const Far mode = mapMode == 3 ? Far::Flat : Far::ByRef;
	LargeRef::FarMap truth = censusMap( mapMode == 3 ? 1 : mapMode );
	const bool placed = LargeRef::fixedPlaced( mode, FIXEDRED || FLATNONERED, FLATRED );
	long steps = 0, stepRefs = 0, kept = 0, hidden = 0, doubles = 0, missing = 0;
	for ( const auto & rt : routes ) {
		int x = rt[0], y = rt[1];
		for ( ;; ) {
			if ( mapMode == 3 )
				truth = flatModeMap( x, y );
			for ( const LargeRef::Row & r : LargeRef::outside( rows, x - 2, y - 2, x + 2, y + 2 ) ) {
				const Far k = truth.at( r.rx, r.ry );
				const bool full = LargeRef::fixedFull( placed, &truth, r.rx, r.ry, FLATRED, FIXEDRED, mode );
				const int seen = ( full ? 1 : 0 ) + ( k == Far::Flat ? 1 : k == Far::ByRef && !full ? 1 : 0 );
				stepRefs++;
				( full ? kept : hidden )++;
				doubles += seen > 1 ? 1 : 0;
				missing += seen < 1 ? 1 : 0;
			}
			steps++;
			if ( x == rt[2] && y == rt[3] )
				break;
			x += ( rt[2] > x ) - ( rt[2] < x );
			y += ( rt[3] > y ) - ( rt[3] < y );
		}
	}
	const bool pass = doubles == 0 && missing == 0 && refused == 0 && kept > 0 && hidden > 0;
	std::printf( "fixed open: %d RNAM subrecords (%d refused), %d rows; %ld steps, %ld step-refs outside the block\n", subs,
		refused, int( rows.size() ), steps, stepRefs );
	std::printf( "fixed open: far %s%s%s; kept full %ld, hidden after the far build %ld step-refs\n",
		mapMode == 1 ? "mixed" : mapMode == 3 ? "flat mode (a .bto-less chunk and a refused ring None)" : "budget",
		FIXEDRED ? " (RED fixedpermode: the mode alone)" : "",
		FLATNONERED ? " (RED fixedflatnone: flat mode places none)" : "", kept, hidden );
	std::printf( "fixed open: two copies %ld, none %ld step-refs -- %s\n", doubles, missing, pass ? "PASS" : "FAIL" );
	return pass ? 0 : 1;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc > 2 && std::strcmp( argv[1], "--rnam" ) == 0 ) {	// lane LARGEREF3: [--far byref|flat|none], byref default
		LargeRef::Far far = LargeRef::Far::ByRef;
		int mapMode = 0;	// lane LARGEREF4: --far mixed|budget (a far map, per cell)
		if ( argc > 4 && std::strcmp( argv[3], "--far" ) == 0 ) {
			far = std::strcmp( argv[4], "flat" ) == 0 ? LargeRef::Far::Flat
				: std::strcmp( argv[4], "none" ) == 0 ? LargeRef::Far::None : LargeRef::Far::ByRef;
			mapMode = std::strcmp( argv[4], "mixed" ) == 0 ? 1 : std::strcmp( argv[4], "budget" ) == 0 ? 2 : 0;
			// lane LARGEREF4: live (budget rebuilt to mixed mid-walk), liveoff (far LOD removed), fixedmixed|fixedbudget
			mapMode = std::strcmp( argv[4], "live" ) == 0 ? 3 : std::strcmp( argv[4], "liveoff" ) == 0 ? 4 : mapMode;
			if ( std::strncmp( argv[4], "fixed", 5 ) == 0 )
				return fixedCensus( argv[2], std::strcmp( argv[4], "fixedbudget" ) == 0 ? 2
					: std::strcmp( argv[4], "fixedflat" ) == 0 ? 3 : 1 );	// lane LARGEREF5: fixedflat
		}
		return census( argv[2], far, mapMode );
	}
	std::vector<LargeRef::Row> rows;
	// grid (10,-2) lists a tower at (12,-3) and one in its own cell; grid (11,-2) lists the tower again
	const auto a = rnam( -2, 10, { ref( 0x100, 12, -3 ), ref( 0x101, 10, -2 ) } );
	const auto b = rnam( -2, 11, { ref( 0x100, 12, -3 ), ref( 0x102, 30, 5 ) } );
	check( LargeRef::decode( a.data(), a.size(), YX, rows ) == 2, "subrecord a: 2 rows" );
	check( LargeRef::decode( b.data(), b.size(), YX, rows ) == 2, "subrecord b: 2 rows" );
	check( rows.size() == 4 && rows[0].gx == 10 && rows[0].gy == -2, "grid read as Y,X: (10,-2)" );
	check( rows[0].rx == 12 && rows[0].ry == -3, "ref cell read as Y,X: (12,-3)" );
	// a 3x3 round (10,-2): the tower stands outside it, the in-cell ref does not count, (30,5) is listed by (11,-2)
	const auto out = LargeRef::outside( rows, 9, -3, 11, -1 );
	check( out.size() == 2, "two refs stand outside the loaded 3x3 (each once)" );
	check( !out.empty() && out[0].ref == 0x100 && out[0].rx == 12 && out[0].ry == -3, "the tower, in its own cell" );
	check( out.size() > 1 && out[1].ref == 0x102 && out[1].rx == 30, "the far one listed by (11,-2)" );
	// a block that holds no listing cell promotes nothing
	check( LargeRef::outside( rows, 0, 0, 2, 2 ).empty(), "a block with no listing cell: none" );
	// a short or lying subrecord is refused, nothing appended
	std::vector<unsigned char> s( a.begin(), a.begin() + 12 );
	const size_t before = rows.size();
	check( LargeRef::decode( s.data(), s.size(), YX, rows ) == -1 && rows.size() == before, "short subrecord refused" );

	// lane LARGEREF2: ownership counts in the streamed view (each wanted large ref drawn exactly once)
	{
		std::vector<LargeRef::Row> own = rows;
		const auto c = rnam( -3, 12, { ref( 0x100, 12, -3 ) } );   // the tower's own cell lists it too
		LargeRef::decode( c.data(), c.size(), true, own );
		LargeRef::Owners o( STREAMRED );
		o.setRows( own );
		o.cell( 10, -2, true );
		check( o.layer().count( 0x100 ) && o.copies( 0x100 ) == 1, "stream: listing cell loaded -> the layer draws the tower once" );
		check( !o.layer().count( 0x101 ) && o.copies( 0x101 ) == 1, "stream: a ref in its own loaded cell -> the cell draws it, not the layer" );
		o.cell( 11, -2, true );
		check( o.listers( 0x100 ) == 2 && o.copies( 0x100 ) == 1, "stream: two listing cells loaded -> still one copy" );
		o.cell( 12, -3, true );
		check( o.copies( 0x100 ) == 1 && !o.layer().count( 0x100 ), "stream: its own cell loads -> the layer lets go (one copy)" );
		o.cell( 12, -3, false );
		check( o.copies( 0x100 ) == 1 && o.layer().count( 0x100 ), "stream: its own cell leaves -> the layer takes it back" );
		o.cell( 10, -2, false );
		o.cell( 11, -2, false );
		check( o.copies( 0x100 ) == 0 && !o.wanted( 0x100 ) && o.layer().empty(), "stream: no listing cell -> none drawn" );
		// a walk: a 5x5 block sliding over the cells, every ref drawn exactly once while wanted, never twice
		int bad = 0, steps = 0;
		std::set<std::pair<int, int>> loaded;
		const int path[][2] = { { 8, -4 }, { 9, -4 }, { 10, -3 }, { 11, -3 }, { 12, -3 }, { 13, -2 }, { 12, -1 }, { 10, -2 }, { 28, 4 }, { 30, 5 }, { 11, -2 } };
		for ( const auto & p : path ) {
			std::set<std::pair<int, int>> next;
			for ( int dy = -2; dy <= 2; dy++ )
				for ( int dx = -2; dx <= 2; dx++ )
					next.insert( { p[0] + dx, p[1] + dy } );
			for ( const auto & k : loaded )
				if ( !next.count( k ) )
					o.cell( k.first, k.second, false );
			for ( const auto & k : next )
				o.cell( k.first, k.second, true );
			loaded = next;
			for ( uint32_t r : { 0x100u, 0x101u, 0x102u } )
				bad += o.copies( r ) != ( o.wanted( r ) ? 1 : 0 );
			steps++;
		}
		std::printf( "stream walk: %d steps, %d wrong copy counts\n", steps, bad );
		check( bad == 0, "stream walk: every wanted ref drawn once, never twice" );
	}
	// lane LARGEREF3 (LARGEREF-FLAT, ruling a): over a flat far field (flattened rings, the game's .bto) a large ref is
	// seen by its far copy alone until its own cell loads; never full beside its far copy
	{
		using LargeRef::Far;
		check( LargeRef::farOf( true, false, true, false, true ) == Far::ByRef, "far: ours instanced -> ByRef" );
		check( LargeRef::farOf( true, false, true, false, false ) == Far::Flat, "far: ours flattened -> Flat" );
		check( LargeRef::farOf( true, false, false, false, true ) == Far::Flat, "far: the game's .bto -> Flat" );
		check( LargeRef::farOf( false, false, true, false, true ) == Far::None, "far: far LOD off -> None" );
		check( LargeRef::farOf( true, true, true, false, true ) == Far::None, "far: NEARONLY -> None" );
		check( LargeRef::farOf( true, false, true, true, true ) == Far::None, "far: red noobjects -> None" );
		check( LargeRef::fullOutside( Far::ByRef, false ) && LargeRef::fullOutside( Far::None, false ),
			"full outside: ByRef and None draw large refs full" );
		std::vector<LargeRef::Row> own = rows;
		const auto c = rnam( -3, 12, { ref( 0x100, 12, -3 ) } );
		LargeRef::decode( c.data(), c.size(), true, own );
		LargeRef::Owners o( false );
		o.setFullOutside( LargeRef::fullOutside( Far::Flat, FLATRED ) );
		o.setRows( own );
		o.cell( 10, -2, true );
		check( o.copies( 0x100 ) == 0 && o.farCopy( 0x100, Far::Flat ) == 1 && o.seen( 0x100, Far::Flat ) == 1,
			"flat: listing cell loaded -> the tower by its far copy alone" );
		check( o.copies( 0x101 ) == 1 && o.seen( 0x101, Far::Flat ) == 1, "flat: a ref in its own loaded cell -> full once" );
		o.cell( 12, -3, true );
		check( o.copies( 0x100 ) == 1 && o.farCopy( 0x100, Far::Flat ) == 0, "flat: its own cell loads -> full, far masked" );
		o.cell( 12, -3, false );
		o.cell( 10, -2, false );
		check( o.seen( 0x100, Far::Flat ) == 1 && o.layer().empty(), "flat: nothing loaded -> the far copy, no layer" );
		// the ByRef reading of the same walk: full while listed, its far rows hidden
		LargeRef::Owners b( STREAMRED );
		b.setRows( own );
		b.cell( 10, -2, true );
		check( b.copies( 0x100 ) == 1 && b.farCopy( 0x100, Far::ByRef ) == 0 && b.seen( 0x100, Far::ByRef ) == 1,
			"byref: listing cell loaded -> full, far rows hidden" );
		/* lane LARGEREF4: the far map. The tower (12,-3) over the game's .bto, a ref in ours, a ref where the budget
		 * left no far objects, all listed by one loaded cell (10,-2) */
		LargeRef::FarMap m;
		m.add( 12, -3, 12, -3, Far::Flat );
		m.add( 10, -10, 30, -3, Far::None );
		m.add( 0, -10, 30, 10, Far::ByRef );
		check( m.at( 12, -3 ) == Far::Flat && m.at( 13, -3 ) == Far::None && m.at( 10, -2 ) == Far::ByRef
			&& m.at( 99, 0 ) == Far::None, "map: first rectangle wins, outside every one -> None" );
		std::vector<LargeRef::Row> mr = own;
		const auto d = rnam( -2, 10, { ref( 0x102, 14, -4 ), ref( 0x103, 11, 2 ) } );
		LargeRef::decode( d.data(), d.size(), true, mr );
		LargeRef::Owners p( false );
		if ( !PERMODERED )
			p.setFarMap( m, FLATRED );
		p.setRows( mr );
		p.cell( 10, -2, true );
		check( p.farAt( 0x100, Far::ByRef ) == ( PERMODERED ? Far::ByRef : Far::Flat ), "map: the tower's kind at its own cell" );
		check( p.seen( 0x100, m.at( 12, -3 ) ) == 1 && p.copies( 0x100 ) == 0,
			"map: over the game's .bto -> its far copy alone (the mixed FO4CS field)" );
		check( p.copies( 0x102 ) == 1 && p.seen( 0x102, m.at( 14, -4 ) ) == 1, "map: no far objects there -> full by the layer" );
		check( p.copies( 0x103 ) == 1 && p.seen( 0x103, m.at( 11, 2 ) ) == 1, "map: over ours -> full, far rows hidden" );
		// lane LARGEREF4 (LARGEREF-MAP-LIVE): what the stream started again after a live far rebuild reads
		LargeRef::FarMap now;
		now.add( 0, 0, 0, 0, Far::Flat );
		check( LargeRef::streamMap( &now, &m, STALERED ) == &now, "live: the restarted stream reads the rebuilt map" );
		check( LargeRef::streamMap( nullptr, &m, STALERED ) == nullptr, "live: far LOD removed -> no map" );
		check( LargeRef::liveFar( false, Far::Flat, STALERED ) == Far::None, "live: far LOD removed -> drawn full (None)" );
		check( LargeRef::liveFar( true, Far::Flat, STALERED ) == Far::Flat, "live: far LOD kept -> the open's kind" );
		// lane LARGEREF4 (LARGEREF-FIXEDOPEN-MIXED): the fixed open, once the far build's map exists
		check( !LargeRef::fixedFull( true, &m, 12, -3, FLATRED, FIXEDRED, Far::ByRef ), "fixed: over the game's .bto -> hidden after the build" );
		check( LargeRef::fixedFull( true, &m, 11, 2, FLATRED, FIXEDRED, Far::ByRef )
				&& LargeRef::fixedFull( true, &m, 14, -4, FLATRED, FIXEDRED, Far::ByRef ),
			"fixed: over ours, or no far objects -> stays full" );
		check( LargeRef::fixedFull( true, nullptr, 12, -3, false, FIXEDRED, Far::ByRef ), "fixed: no map (far LOD off) -> the mode" );
		check( !LargeRef::fixedFull( false, &m, 11, 2, false, FIXEDRED, Far::ByRef ), "fixed: not placed by the mode -> not full" );
		// lane LARGEREF5 (FIXEDOPEN-FLATMODE): flat mode places them all; the map keeps the None ones full
		const bool flatPlaced = LargeRef::fixedPlaced( Far::Flat, FLATNONERED, false );
		check( flatPlaced, "fixed flat mode: every outside large ref placed before the far build" );
		check( LargeRef::fixedFull( flatPlaced, &m, 14, -4, false, false, Far::Flat ),
			"fixed flat mode: own cell None in the map -> stays full" );
		check( !LargeRef::fixedFull( flatPlaced, &m, 12, -3, false, false, Far::Flat ), "fixed flat mode: own cell flat -> hidden" );
		check( !LargeRef::fixedFull( true, nullptr, 12, -3, false, false, Far::Flat ), "fixed flat mode: no map -> the mode (hidden)" );
		check( !LargeRef::fixedPlaced( Far::Flat, true, false ) && LargeRef::fixedPlaced( Far::ByRef, true, false ),
			"fixed: by the mode (fixedpermode, the stream's open) -> flat places none, byref all" );
	}
	// lane LARGEREF2 (LARGEREF-INI): the ini line largeRefsWanted reads, through the same reader and arguments
	{
		auto ini = []( const std::vector<std::string> & lines ) {
			std::string sec;
			float v = 1.0f;   // the exe default: off
			for ( const std::string & l : lines )
				GameIniKey::floatLine( l, "[general]", "bdisablelargerefs", sec, v );
			return v;
		};
		check( ini( { "[General]", "bDisableLargeRefs=0" } ) == 0.0f, "ini: [General] bDisableLargeRefs=0 -> on" );
		check( ini( { "[Display]", "bDisableLargeRefs=0" } ) == 1.0f, "ini: another section -> the default (off)" );
		check( ini( { "[General]", "bDisableLargeRefs=0", "[General]", "  bdisablelargerefs = 1\r" } ) == 1.0f,
			"ini: a later line wins" );
		check( ini( {} ) == 1.0f, "ini: no line -> off" );
	}
	std::printf( "largeref: %d failed -- %s\n", fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
