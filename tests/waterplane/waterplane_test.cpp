// Lane WATERUNDER5: the placed-water fallback of the under-water test (src/gl/waterplanefit.h).
// g++ -std=c++17 -I<tree>/src tests/waterplane/waterplane_test.cpp -o waterplane_test && ./waterplane_test
// Red: -DWW_WATERPLANE_OLD_RED runs WATERUNDER1's rule (bounding box, top vertex, no room test): it must fail.
// Red: -DWW_WATERROOM_ROOF_RED runs WATERUNDER5's room test (any roof over the camera): the hull/pier cases must fail.
#include "gl/waterplanefit.h"

#include <cstdio>

#ifdef WW_WATERPLANE_OLD_RED
static const bool kOld = true;
#else
static const bool kOld = false;
#endif
#ifdef WW_WATERROOM_ROOF_RED
static const bool kRoofOnly = true;
#else
static const bool kRoofOnly = false;
#endif

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

static void quad( std::vector<float> & t, float x0, float y0, float x1, float y1, float z00, float z10, float z01, float z11 )
{
	t.insert( t.end(), { x0, y0, z00, x1, y0, z10, x0, y1, z01 } );
	t.insert( t.end(), { x0, y1, z01, x1, y0, z10, x1, y1, z11 } );
}

// the room test as the cell view runs it: off under the old rule
static bool roofed( const std::vector<float> & roof, float x, float y, float z, float h )
{
	if ( kOld )
		return false;
	return kRoofOnly ? wwWaterRoofOver( roof.data(), roof.size() / 9, x, y, z, h )
		: wwWaterRoomOver( roof.data(), roof.size() / 9, x, y, z, h );
}

// an axis box's sides and top (no floor): x0..x1, y0..y1, z0..z1
static void box( std::vector<float> & t, float x0, float y0, float z0, float x1, float y1, float z1, bool top = true )
{
	if ( top )
		quad( t, x0, y0, x1, y1, z1, z1, z1, z1 );
	auto wall = [&]( float ax, float ay, float bx, float by ) {
		t.insert( t.end(), { ax, ay, z0, bx, by, z0, ax, ay, z1 } );
		t.insert( t.end(), { ax, ay, z1, bx, by, z0, bx, by, z1 } );
	};
	wall( x0, y0, x1, y0 );
	wall( x1, y0, x1, y1 );
	wall( x1, y1, x0, y1 );
	wall( x0, y1, x0, y0 );
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	float h = 0.0f;

	// 1. an L-shaped pond: 0..2000 x 0..1000 plus 0..1000 x 1000..2000, flat at z 100. (1500, 1500) is in its box only.
	{
		std::vector<float> t;
		quad( t, 0, 0, 2000, 1000, 100, 100, 100, 100 );
		quad( t, 0, 1000, 1000, 2000, 100, 100, 100, 100 );
		const std::vector<WwWaterPoly> ps = { wwWaterPolyFrom( t, 0 ) };
		check( wwWaterPolyPick( ps, 500, 500, 50, 1000, kOld, h ) == 0 && std::fabs( h - 100 ) < 1e-3f,
			"L pond: a point in the polygon is under its water" );
		check( wwWaterPolyPick( ps, 1500, 1500, 50, 1000, kOld, h ) < 0,
			"L pond: the empty corner of its box is not under water" );
	}

	// 2. a gently tilted shape (one plane, 1 unit over 4096): the height at the low end is the plane, not the top vertex
	{
		std::vector<float> t;
		quad( t, 0, 0, 4096, 4096, 0.0f, 1.0f, 0.0f, 1.0f );
		const std::vector<WwWaterPoly> ps = { wwWaterPolyFrom( t, 0 ) };
		const bool hit = wwWaterPolyPick( ps, 0, 2048, -10, 1000, kOld, h ) == 0;
		check( hit && std::fabs( h - 0.0f ) < 1e-3f, "tilted: the mean plane at the low end (0), not the top vertex (1)" );
		// a camera 0.5 above the low end is above the water
		check( wwWaterPolyPick( ps, 0, 2048, 0.5f, 1000, kOld, h ) < 0, "tilted: 0.5 above the low end is above water" );
	}

	// 3. one shape of two flat quads at 0 and 300 (a bucket of cell water at two heights): each reads its own height
	{
		std::vector<float> t;
		quad( t, 0, 0, 1000, 1000, 0, 0, 0, 0 );
		quad( t, 2000, 0, 3000, 1000, 300, 300, 300, 300 );
		const WwWaterPoly p = wwWaterPolyFrom( t, 0 );
		check( p.resid > kWwWaterFlatTol, "stepped: the residual marks it as not one plane" );
		const std::vector<WwWaterPoly> ps = { p };
		check( wwWaterPolyPick( ps, 500, 500, -100, 1000, kOld, h ) == 0 && std::fabs( h - 0.0f ) < 1e-3f,
			"stepped: the low quad reads 0" );
		check( wwWaterPolyPick( ps, 2500, 500, 100, 1000, kOld, h ) == 0 && std::fabs( h - 300.0f ) < 1e-3f,
			"stepped: the high quad reads 300" );
		check( wwWaterPolyPick( ps, 500, 500, 100, 1000, kOld, h ) < 0, "stepped: 100 over the low quad is above water" );
	}

	// 4. the lowest covering surface wins
	{
		std::vector<float> a, b;
		quad( a, 0, 0, 1000, 1000, 500, 500, 500, 500 );
		quad( b, 0, 0, 1000, 1000, 200, 200, 200, 200 );
		const std::vector<WwWaterPoly> ps = { wwWaterPolyFrom( a, 0 ), wwWaterPolyFrom( b, 1 ) };
		check( wwWaterPolyPick( ps, 500, 500, 100, 1000, kOld, h ) == 1 && std::fabs( h - 200 ) < 1e-3f,
			"two surfaces: the lowest above the camera" );
	}

	// 5. a room below a pond: a ceiling at z 50 between the camera (z 0) and the water (z 100)
	{
		std::vector<float> t, roof;
		quad( t, 0, 0, 1000, 1000, 100, 100, 100, 100 );
		quad( roof, 200, 200, 800, 800, 50, 50, 50, 50 );
		const std::vector<WwWaterPoly> ps = { wwWaterPolyFrom( t, 0 ) };
		const bool hit = wwWaterPolyPick( ps, 500, 500, 0, 1000, kOld, h ) == 0;
		check( hit && roofed( roof, 500, 500, 0, h ), "room: a ceiling between camera and water means not under water" );
		check( hit && !roofed( roof, 100, 100, 0, h ), "room: beside the ceiling the camera is in the water" );
		// a dock above the surface does not count
		std::vector<float> dock;
		quad( dock, 0, 0, 1000, 1000, 150, 150, 150, 150 );
		check( !wwWaterRoofOver( dock.data(), dock.size() / 9, 500, 500, 0, 100 ), "room: a dock above the water is no roof" );
	}

	// 6. (WATERUNDER6) submerged geometry over the camera is no room; a closed basement still is
	{
		std::vector<float> t;
		quad( t, 0, 0, 4000, 4000, 100, 100, 100, 100 );
		const std::vector<WwWaterPoly> ps = { wwWaterPolyFrom( t, 0 ) };
		const bool hit = wwWaterPolyPick( ps, 2000, 2000, 0, 1000, kOld, h ) == 0;
		// a sunken hull: 400 wide, 1000 long, bottom at z 60, sides up to 120 (above the surface)
		std::vector<float> hull;
		box( hull, 1800, 1500, 60, 2200, 2500, 120 );
		quad( hull, 1800, 1500, 2200, 2500, 60, 60, 60, 60 );
		check( hit && !roofed( hull, 2000, 2000, 0, h ), "hull: under a sunken hull the camera is in the water" );
		// a pier's underside: a slab at z 80, 200 wide, 4000 long, with two piles beside the camera
		std::vector<float> pier;
		quad( pier, 0, 1900, 4000, 2100, 80, 80, 80, 80 );
		box( pier, 1500, 1950, -500, 1520, 1970, 80, false );
		box( pier, 2480, 1950, -500, 2500, 1970, 80, false );
		check( hit && !roofed( pier, 2000, 2000, 0, h ), "pier: under a pier's underside the camera is in the water" );
		// a deep camera under a small ceiling far below the surface (the cone escapes past its edge)
		std::vector<float> slab;
		quad( slab, 1900, 1900, 2100, 2100, -300, -300, -300, -300 );
		check( wwWaterPolyPick( ps, 2000, 2000, -400, 1000, kOld, h ) == 0 && !roofed( slab, 2000, 2000, -400, h ),
			"slab: 100 under a small sunken slab, 500 deep, the camera is in the water" );
		// a closed basement 1000 x 1000, walls z 0..600, ceiling 600, the camera at z 10 in the middle
		std::vector<float> room;
		box( room, 1500, 1500, 0, 2500, 2500, 600 );
		std::vector<float> deep;
		quad( deep, 0, 0, 4000, 4000, 1000, 1000, 1000, 1000 );
		const std::vector<WwWaterPoly> pd = { wwWaterPolyFrom( deep, 0 ) };
		const bool hitD = wwWaterPolyPick( pd, 2000, 2000, 10, 2000, kOld, h ) == 0;
		check( hitD && ( kOld || roofed( room, 2000, 2000, 10, h ) ), "basement: a closed room under deep water is a room" );
		// the same room with one wall gone is open to the water
		std::vector<float> open;
		quad( open, 1500, 1500, 2500, 2500, 600, 600, 600, 600 );
		box( open, 1500, 1500, 0, 2500, 2500, 600, false );
		open.resize( open.size() - 18 );	// drop the last wall (x0 side)
		check( hitD && !roofed( open, 2000, 2000, 10, h ), "basement: with a wall gone the camera is in the water" );
	}

	std::printf( "%s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
