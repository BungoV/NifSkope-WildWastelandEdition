/* lane CAMGATE1: unit test of the camera bounds rule (src/cellcambounds.h, namespace WwCamBounds), no Qt.
 *   g++ -std=c++17 -I<tree>/src tests/cambounds/cambounds_test.cpp -o cambounds_test         green: every check passes
 *   ... -DCAMBOUNDS_RED_UNDER   the old rule without the ground test: check "under" must FAIL
 *   ... -DCAMBOUNDS_RED_OUTSIDE the old rule without the block test: check "outside" must FAIL
 *   cambounds_test --land <33 lines of 33 heights> <cx> <cy> <points file: x y per line>
 *       prints "x y ground" per point (the twin check against notes/camgate1/tools/camgate2.py ground()) */
#define CAMBOUNDS_PURE
#include "cellcambounds.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <utility>

using namespace WwCamBounds;

static int g_fail = 0, g_checks = 0;
static void check( const char * name, bool ok, const char * what )
{
	g_checks++;
	if ( !ok )
		g_fail++;
	std::printf( "%s %-12s %s\n", ok ? "ok  " : "FAIL", name, what );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc == 6 && !std::strcmp( argv[1], "--land" ) ) {
		static float H[kGrid][kGrid];
		std::ifstream f( argv[2] );
		for ( int r = 0; r < kGrid; r++ )
			for ( int c = 0; c < kGrid; c++ )
				if ( !( f >> H[r][c] ) )
					return 2;
		const int cx = std::atoi( argv[3] ), cy = std::atoi( argv[4] );
		std::ifstream p( argv[5] );
		double x, y;
		while ( p >> x >> y )
			std::printf( "%.3f %.3f %.6f\n", x, y, groundIn( H, cx, cy, x, y ) );
		return 0;
	}

	// a 3 x 3 block round cell (10, -4): a plane z = 0.25 x - 0.5 y + 300 (bilinear reproduces a plane exactly), cell (11, -4)
	// has no LAND, and one cell of the 5 x 5 ring (13, -4) has LAND for the roaming check
	static float P[kGrid][kGrid];
	std::map<std::pair<int, int>, bool> haveLand;
	for ( int x = 8; x <= 13; x++ )
		for ( int y = -6; y <= -2; y++ )
			haveLand[{ x, y }] = !( x == 11 && y == -4 );
	auto plane = []( double x, double y ) { return 0.25 * x - 0.5 * y + 300.0; };
	std::map<std::pair<int, int>, std::pair<bool, float ( * )[kGrid]>> grids;
	static float G[64][kGrid][kGrid];
	int used = 0;
	auto land = [&]( int cx, int cy ) -> const float ( * )[kGrid] {
		auto it = haveLand.find( { cx, cy } );
		if ( it == haveLand.end() || !it->second )
			return nullptr;
		auto g = grids.find( { cx, cy } );
		if ( g == grids.end() ) {
			float ( *H )[kGrid] = G[used++];
			for ( int r = 0; r < kGrid; r++ )
				for ( int c = 0; c < kGrid; c++ )
					H[r][c] = float( plane( cx * kCellUnits + c * 128.0, cy * kCellUnits + r * 128.0 ) );
			g = grids.emplace( std::make_pair( cx, cy ), std::make_pair( true, H ) ).first;
		}
		return g->second.second;
	};
	(void) P;
	Box b;
	b.cx = 10;
	b.cy = -4;
	b.half = 1;
	const double x0 = 10.5 * kCellUnits, y0 = -3.5 * kCellUnits;	// the centre cell's middle
	const double g0 = plane( x0, y0 );
	double g = 0.0;

	check( "ground", judge( b, land, x0, y0, g0 + 500.0, &g ) == Verdict::Ok && std::fabs( g - g0 ) < 1e-3,
		"bilinear on a plane = the plane (the middle of the centre cell)" );
	{
		const double x = 10 * kCellUnits + 1000.37, y = -4 * kCellUnits + 77.9;
		double gg = 0.0;
		judge( b, land, x, y, 1e6, &gg );
		check( "plane", std::fabs( gg - plane( x, y ) ) < 1e-2, "an off-grid point reads the plane" );
	}
	check( "above", judge( b, land, x0, y0, g0 + kClear + 1.0 ) == Verdict::Ok, "eye at ground + 51: ok" );
	check( "under", judge( b, land, x0, y0, g0 - 100.0 ) == Verdict::Under, "eye at ground - 100: refused (the brief's red row)" );
	check( "underedge", judge( b, land, x0, y0, g0 + kClear - 1.0 ) == Verdict::Under, "eye at ground + 49: refused" );
	check( "outside", judge( b, land, ( 12 + 0.5 ) * kCellUnits, y0, 1e5 ) == Verdict::Outside,
		"eye one cell east of the 3 x 3, high in the air: refused" );
	check( "edgein", judge( b, land, 9 * kCellUnits + 0.5, y0, plane( 9 * kCellUnits + 0.5, y0 ) + 200.0 ) == Verdict::Ok,
		"eye half a unit inside the block's west edge: ok" );
	check( "edgeout", judge( b, land, 9 * kCellUnits - 0.5, y0, plane( 9 * kCellUnits - 0.5, y0 ) + 200.0 ) == Verdict::Outside,
		"eye half a unit outside the block's west edge (cell 8 has LAND): refused" );
	check( "noland", judge( b, land, 11.5 * kCellUnits, y0, 1e5 ) == Verdict::NoLand, "eye over a loaded cell with no LAND: refused" );
	Box roam = b;
	roam.roam = true;
	check( "roam", judge( roam, land, 13.5 * kCellUnits, y0, plane( 13.5 * kCellUnits, y0 ) + 200.0 ) == Verdict::Ok,
		"a streamed document: any cell with LAND" );
	check( "roamunder", judge( roam, land, 13.5 * kCellUnits, y0, plane( 13.5 * kCellUnits, y0 ) - 10.0 ) == Verdict::Under,
		"a streamed document: still never under the ground" );
	check( "origin", judge( b, land, 0.0, 0.0, 0.0 ) == Verdict::Outside, "a fresh document's world origin: refused" );
	check( "nan", judge( b, land, std::nan( "" ), y0, 1e4 ) == Verdict::Outside, "a NaN eye: refused" );
	check( "cellof", cellOf( -0.5 ) == -1 && cellOf( 4095.9 ) == 0 && cellOf( -4096.0 ) == -1 && cellOf( -4096.1 ) == -2,
		"cells floor toward minus infinity (the negative half of the map)" );
	std::printf( "%d checks, %d failures\n%s\n", g_checks, g_fail, g_fail ? "FAIL" : "PASS" );
	return g_fail ? 1 : 0;
}
