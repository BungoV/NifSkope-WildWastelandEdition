// Lane WATERUNDER5: the placed-water fallback of the under-water test (src/gl/waterplanefit.h).
// g++ -std=c++17 -I<tree>/src tests/waterplane/waterplane_test.cpp -o waterplane_test && ./waterplane_test
// Red: -DWW_WATERPLANE_OLD_RED runs WATERUNDER1's rule (bounding box, top vertex, no room test): it must fail.
#include "gl/waterplanefit.h"

#include <cstdio>

#ifdef WW_WATERPLANE_OLD_RED
static const bool kOld = true;
#else
static const bool kOld = false;
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
	return !kOld && wwWaterRoofOver( roof.data(), roof.size() / 9, x, y, z, h );
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

	std::printf( "%s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
