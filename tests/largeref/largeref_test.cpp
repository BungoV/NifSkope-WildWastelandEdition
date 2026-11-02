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
static int census( const char * path, LargeRef::Far far )
{
	std::FILE * f = std::fopen( path, "rb" );
	if ( !f ) {
		std::printf( "census: cannot read %s\n", path );
		return 2;
	}
	std::vector<LargeRef::Row> rows;
	int subs = 0, refused = 0;
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
	LargeRef::Owners o( STREAMRED );
	o.setFullOutside( LargeRef::fullOutside( far, FLATRED ) );	// lane LARGEREF3: as cellstream.cpp sets it
	o.setRows( rows );
	std::set<uint32_t> all;
	for ( const LargeRef::Row & r : rows )
		all.insert( r.ref );
	// routes (block centres): Sanctuary east, Concord to downtown, along downtown, the south coast up north
	const int routes[][4] = { { -17, 23, 12, 23 }, { -15, 17, 2, -2 }, { -10, -2, 30, -2 }, { 25, -25, 25, 20 } };
	std::set<std::pair<int, int>> loaded;
	long events = 0, twice = 0, stepsBad = 0, steps = 0;
	size_t maxLayer = 0, maxWanted = 0;
	std::set<uint32_t> everLayer;
	auto watch = [&]() {
		events++;
		for ( uint32_t r : o.layer() )   // only a layer ref can be a second copy (full + full, or full + far)
			twice += o.seen( r, far ) > 1 ? 1 : 0;
	};
	long farOnly = 0;   // lane LARGEREF3: step x ref seen as its far copy alone while a loaded cell lists it
	for ( const auto & rt : routes ) {
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
				bad += far == LargeRef::Far::None ? o.copies( r ) != ( w ? 1 : 0 ) : o.seen( r, far ) != 1;
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
	const bool floor = flat ? ( maxLayer == 0 && farOnly > 0 ) || FLATRED : maxLayer > 0;
	const bool pass = twice == 0 && stepsBad == 0 && refused == 0 && floor;
	std::printf( "census: far %s; listed refs shown by the far copy alone %ld step-refs\n",
		far == LargeRef::Far::None ? "none" : flat ? "flat" : "byref", farOnly );
	std::printf( "census: refs with two copies after a cell event %ld; steps with a wrong copy count %ld -- %s\n", twice,
		stepsBad, pass ? "PASS" : "FAIL" );
	return pass ? 0 : 1;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc > 2 && std::strcmp( argv[1], "--rnam" ) == 0 ) {	// lane LARGEREF3: [--far byref|flat|none], byref default
		LargeRef::Far far = LargeRef::Far::ByRef;
		if ( argc > 4 && std::strcmp( argv[3], "--far" ) == 0 )
			far = std::strcmp( argv[4], "flat" ) == 0 ? LargeRef::Far::Flat
				: std::strcmp( argv[4], "none" ) == 0 ? LargeRef::Far::None : LargeRef::Far::ByRef;
		return census( argv[2], far );
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
