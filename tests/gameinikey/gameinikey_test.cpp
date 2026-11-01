// Lane PERFFIX4: src/gameinikey.h on the game's own preset inis (copied lines) and edge cases.
// g++ -std=c++17 -I<tree>/src tests/gameinikey/gameinikey_test.cpp [-DGAMEINIKEY_RED: the old fixed 3500 fade]
// Optional argv[1..]: real ini files (the game folder's Low/Medium/High/Ultra.ini) checked for 1000/2300/4000/7000.
#include "gameinikey.h"

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

static float fadeOf( const std::vector<std::string> & lines )
{
#ifdef GAMEINIKEY_RED
	(void)lines;
	return 3500.0f;	// the old code: the engine default whatever the ini says
#else
	float v = 3500.0f;
	std::string sec;
	for ( const std::string & l : lines )
		GameIniKey::floatLine( l, "[grass]", "fgrassstartfadedistance", sec, v );
	return v;
#endif
}

int main( int argc, char ** argv )
{
	check( fadeOf( { "[Grass]", "fGrassStartFadeDistance=7000.0000" } ) == 7000.0f, "Ultra.ini line read" );
	check( fadeOf( { "[GRASS]\r", "  FGRASSSTARTFADEDISTANCE = 2300.0000 \r" } ) == 2300.0f, "case, spaces, CRLF" );
	check( fadeOf( { "[Display]", "fGrassStartFadeDistance=9999", "[Grass]", "bAllowCreateGrass=1" } ) == 3500.0f,
		"the key in another section is not the grass fade" );
	check( fadeOf( { "[Grass]", ";fGrassStartFadeDistance=1000", "fGrassStartFadeDistance=4000" } ) == 4000.0f,
		"a comment line does not count" );
	check( fadeOf( { "[Grass]", "fGrassStartFadeDistance=1000", "[Grass]", "fGrassStartFadeDistance=4000" } ) == 4000.0f,
		"the later line wins (Prefs over the game's default ini)" );
	const float want[4] = { 1000.0f, 2300.0f, 4000.0f, 7000.0f };
	for ( int i = 1; i < argc && i <= 4; i++ ) {
		std::ifstream f( argv[i] );
		std::vector<std::string> lines;
		for ( std::string l; std::getline( f, l ); )
			lines.push_back( l );
		char msg[256];
		std::snprintf( msg, sizeof msg, "%s reads %.0f", argv[i], double( want[i - 1] ) );
		check( !lines.empty() && fadeOf( lines ) == want[i - 1], msg );
	}
	std::printf( "GAMEINIKEY %s (%d checks, %d failures)\n", fails ? "FAIL" : "PASS", checks, fails );
	return fails ? 1 : 0;
}
