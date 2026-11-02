// lane GEOLOD1 (LARGEREF1): the RNAM decode + the large-ref rule of src/largeref.h.
// g++ -std=c++17 -I<tree>/src tests/largeref/largeref_test.cpp; -DLARGEREF_RED reads X,Y (the red) and must fail.
#include "largeref.h"

#include <cstdio>
#include <cstring>

#ifdef LARGEREF_RED
static const bool YX = false;
#else
static const bool YX = true;
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

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
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
	std::printf( "largeref: %d failed -- %s\n", fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
