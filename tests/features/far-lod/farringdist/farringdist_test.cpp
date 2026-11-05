// Lane FARPRESET1: src/features/far-lod/farringdist.h on the game's own preset inis and the ring 1 area floor.
// g++ -std=c++17 -I<tree>/src tests/features/far-lod/farringdist/farringdist_test.cpp [-DFARRINGDIST_RED: Ultra rings on every preset]
// Optional argv[1..4]: the game folder's Low/Medium/High/Ultra.ini, each checked against kPreset.
#include "features/far-lod/farringdist.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int fails = 0, checks = 0;
static void check( bool ok, const char * what )
{
	checks++;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

//! What the cell view builds from these ini lines (the red: the old fixed Ultra distances)
static FarRingDist::Dist distOf( const std::vector<std::string> & lines )
{
	FarRingDist::Dist d;
#ifdef FARRINGDIST_RED
	(void)lines;
#else
	std::string sec;
	for ( const std::string & l : lines )
		FarRingDist::line( l, sec, d );
	d = FarRingDist::ordered( d );
#endif
	return d;
}

static bool same( const FarRingDist::Dist & d, int p )
{
	return d.l0 == FarRingDist::kPreset[p][0] && d.l1 == FarRingDist::kPreset[p][1] && d.l2 == FarRingDist::kPreset[p][2]
		&& d.max == FarRingDist::kPreset[p][3];
}

static std::vector<std::string> presetLines( int p )
{
	char b[4][64];
	std::snprintf( b[0], 64, "fBlockMaximumDistance=%.4f", double( FarRingDist::kPreset[p][3] ) );
	std::snprintf( b[1], 64, "fBlockLevel2Distance=%.4f", double( FarRingDist::kPreset[p][2] ) );
	std::snprintf( b[2], 64, "fBlockLevel1Distance=%.4f", double( FarRingDist::kPreset[p][1] ) );
	std::snprintf( b[3], 64, "fBlockLevel0Distance=%.4f", double( FarRingDist::kPreset[p][0] ) );
	return { "[TerrainManager]", b[0], b[1], b[2], b[3] };
}

/* Ring 1 (LOD4 objects, sheet dim 8) in cells at c3 (Concord, block centre -15,17, grid half 2): cellfarlod.cpp's reach
 * max(h + 2, cellsTo(l0)), snapped out to whole dim-8 tiles, less ring 0 (-18,14 .. -12,20 as the c3 notes print it). */
static int ring1Cells( const FarRingDist::Dist & d )
{
	const int cx = -15, cy = 17, h = 2, dim = 8;
	const int r = std::max( h + 2, FarRingDist::cellsTo( d.l0 ) );
	auto lo = [dim]( int v ) { return v >= 0 ? v / dim * dim : -( ( -v + dim - 1 ) / dim ) * dim; };
	auto hi = [&lo, dim]( int v ) { return lo( v + dim ) - 1; };
	const int x0 = lo( cx - r ), x1 = hi( cx + r ), y0 = lo( cy - r ), y1 = hi( cy + r );
	return ( x1 - x0 + 1 ) * ( y1 - y0 + 1 ) - 7 * 7;
}

int main( int argc, char ** argv )
{
	for ( int p = 0; p < 4; p++ ) {
		char msg[96];
		std::snprintf( msg, sizeof msg, "preset %d lines read as kPreset[%d]", p, p );
		check( same( distOf( presetLines( p ) ), p ), msg );
	}
	check( same( distOf( { "[Display]", "fBlockLevel0Distance=1" } ), 3 ), "a key in another section: Ultra kept" );
	check( distOf( { "[terrainmanager]\r", "  FBLOCKLEVEL0DISTANCE = 15000 \r" } ).l0 == 15000.0f, "case, spaces, CRLF" );
	check( distOf( { "[TerrainManager]", ";fBlockLevel0Distance=1000" } ).l0 == 60000.0f, "a comment does not count" );
	check( distOf( { "[TerrainManager]", "fBlockLevel0Distance=15000", "[TerrainManager]", "fBlockLevel0Distance=30000" } ).l0
			== 30000.0f, "the later line wins (Prefs over the default ini)" );
	{
		const FarRingDist::Dist d = distOf( { "[TerrainManager]", "fBlockLevel0Distance=100000", "fBlockMaximumDistance=0" } );
		check( d.l1 >= d.l0 && d.l2 >= d.l1 && d.max >= d.l2 && d.max == 250000.0f, "out of order / 0 values kept ordered" );
	}
	check( FarRingDist::presetByName( "LOW" ) == 0 && FarRingDist::presetByName( "3" ) == 3
			&& FarRingDist::presetByName( "epic" ) == -1, "preset names" );
	// the floor (RESEARCH G2): Low ring 1 <= 15 % of Ultra's; the red (Ultra on every preset) reads 100 %
	const int low = ring1Cells( distOf( presetLines( 0 ) ) ), ultra = ring1Cells( FarRingDist::preset( 3 ) );
	{
		char msg[128];
		std::snprintf( msg, sizeof msg, "c3 ring 1 cells Low %d vs Ultra %d = %.1f %% (floor <= 15 %%)", low, ultra,
			100.0 * low / ultra );
		check( low * 100 <= ultra * 15, msg );
	}
	for ( int i = 1; i < argc && i <= 4; i++ ) {
		std::ifstream f( argv[i] );
		std::vector<std::string> lines;
		for ( std::string l; std::getline( f, l ); )
			lines.push_back( l );
		const FarRingDist::Dist d = distOf( lines );
		char msg[300];
		std::snprintf( msg, sizeof msg, "%s reads %.0f/%.0f/%.0f/%.0f", argv[i], double( d.l0 ), double( d.l1 ),
			double( d.l2 ), double( d.max ) );
		check( !lines.empty() && same( d, i - 1 ), msg );
	}
	std::printf( "FARRINGDIST %s (%d checks, %d failures)\n", fails ? "FAIL" : "PASS", checks, fails );
	return fails ? 1 : 0;
}
