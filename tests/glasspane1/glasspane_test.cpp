// Lane GLASSPANE1 (FIX1030B, 2026-10-30): the effect shader's env mask channel (src/gl/glasspanerules.h), judged on
// the game's own effect materials. Usage: glasspane_test <census.tsv> (tests/glasspane1/glass_census.py writes it).
//   rule:   the default channel is R; WW_FX_ENVMASK_RED=gloss gives G, =noenv no cube.
//   census: every effect material with an env mask: the channel the default reads has the mean the game's read (R)
//           has, within 0.02, and at least 50 masks were decoded (an empty census cannot pass).
// Green prints "GLASSPANE PASS <checks>" and exits 0; the red build (-DGLASSPANE_RED, the base's G default) must print
// "FAIL rule:" and "FAIL census:" lines and exit 1.
#include "gl/glasspanerules.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int g_fail = 0, g_checks = 0;

static void check( bool ok, const char * group, const std::string & what )
{
	g_checks++;
	if ( !ok ) {
		g_fail++;
		std::printf( "FAIL %s: %s\n", group, what.c_str() );
	}
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	check( wwFxEnvMaskRule( nullptr ) == WwFxEnvMask::R, "rule", "unset -> R (the game's)" );
	check( wwFxEnvMaskRule( "" ) == WwFxEnvMask::R, "rule", "empty -> R" );
	check( wwFxEnvMaskRule( "gloss" ) == WwFxEnvMask::G, "rule", "gloss -> G (the old read, red)" );
	check( wwFxEnvMaskRule( "noenv" ) == WwFxEnvMask::None, "rule", "noenv -> no cube (red)" );

	if ( argc < 2 ) {
		std::printf( "FAIL census: no census file given\n" );
		return 1;
	}
	std::ifstream in( argv[1] );
	if ( !in ) {
		std::printf( "FAIL census: cannot open %s\n", argv[1] );
		return 1;
	}
	const WwFxEnvMask ch = wwFxEnvMaskRule( nullptr );
	int rows = 0, off = 0, offPanes = 0, panes = 0;
	double sumOurs = 0, sumGame = 0;
	std::string line, worst;
	double worstD = 0;
	while ( std::getline( in, line ) ) {
		if ( line.empty() || line[0] == '#' )
			continue;
		std::vector<std::string> f;
		std::stringstream ss( line );
		std::string c;
		while ( std::getline( ss, c, '\t' ) )
			f.push_back( c );
		if ( f.size() < 5 || f[0] != "bgem" )
			continue;
		const double R = std::atof( f[3].c_str() ), G = std::atof( f[4].c_str() );
		if ( f[3] == "nan" || f[4] == "nan" || !std::isfinite( R ) || !std::isfinite( G ) )
			continue;	// a mask the census could not decode (named in its own output)
		rows++;
		const bool pane = f[2] == "1";
		panes += pane;
		const double ours = ch == WwFxEnvMask::R ? R : ch == WwFxEnvMask::G ? G : 0.0;
		sumOurs += ours;
		sumGame += R;
		const double d = std::fabs( ours - R );
		if ( d > 0.02 ) {
			off++;
			offPanes += pane;
			if ( d > worstD ) {
				worstD = d;
				worst = f[1];
			}
		}
	}
	char buf[512];
	std::snprintf( buf, sizeof buf, "masks decoded %d (>= 50 needed)", rows );
	check( rows >= 50, "census", buf );
	std::snprintf( buf, sizeof buf, "%d of %d masks (%d of %d panes) read off the game's R by > 0.02; worst %.3f %s; "
		"cube scale ours/game over all %.3f", off, rows, offPanes, panes, worstD, worst.c_str(),
		sumGame > 0 ? sumOurs / sumGame : 0.0 );
	check( off == 0, "census", buf );
	std::printf( "census: %s\n", buf );
	if ( g_fail ) {
		std::printf( "GLASSPANE FAIL %d of %d\n", g_fail, g_checks );
		return 1;
	}
	std::printf( "GLASSPANE PASS %d\n", g_checks );
	return 0;
}
