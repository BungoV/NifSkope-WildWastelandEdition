// Lane VKFLIGHT6 (2026-11-16): the sorted zone's twin draws and GL's draws by the refusal before them
// (src/features/near/cellglzone.h: zoneWhy / zoneVk / countGl / zoneLine / zoneReset).
// Usage: sortedview [red]   red = a GL draw that keeps the refusal of the draw before it (the why never cleared): must FAIL.
#include "features/near/cellglzone.h"

#include <cstdio>
#include <cstring>
#include <string>

static bool red = false;

// the red: GL's draw counted as before, but the refusal is left for the next draw
static void gl( long long calls )
{
	if ( !red ) {
		WwBl::countGl( calls );
		return;
	}
	WwBl::GlZones & z = WwBl::glZones();
	const std::string keep = z.why;
	const bool named = z.whyNamed;
	WwBl::countGl( calls );
	z.why = keep;
	z.whyNamed = named;
}

int main( int argc, char ** argv )
{
	red = argc > 1 && !std::strcmp( argv[1], "red" );
	int fails = 0, n = 0;
	auto row = [&]( bool ok, const std::string & what ) {
		n++;
		fails += !ok;
		std::printf( "  %s %s\n", ok ? "ok  " : "FAIL", what.c_str() );
	};
	WwBl::GlZones & z = WwBl::glZones();
	WwBl::zoneReset();
	const int prev = WwBl::glZoneSet( WwBl::ZoneSorted );
	// 1. glass (fo4_cellbl): vkCellDraw says "no twin" for the bl program, vkBlDraw's own refusal wins, GL draws it
	WwBl::zoneWhy( "no twin: fo4_cellbl.prog", false );
	WwBl::zoneWhy( "a blended rigid shape (the opaque route only)", true );
	gl( 1 );
	// 2. an effect on its twin: drawn on Vulkan, nothing pending after it
	WwBl::zoneWhy( "no twin: fo4_effectcell.prog", false );
	WwBl::zoneVk( 1 );
	// 3. a program with no twin at all: both twins say "no twin" (the first stays), GL draws 2 calls
	WwBl::zoneWhy( "no twin: sky.prog", false );
	WwBl::zoneWhy( "no twin: sky.prog (bl)", false );
	gl( 2 );
	// 4. a GL draw no twin was asked about
	gl( 1 );
	// 5. a named refusal then the weak one after it: the named stays
	WwBl::zoneWhy( "the RT key differs", true );
	WwBl::zoneWhy( "no twin: fo4_effectcell.prog", false );
	gl( 1 );
	// 6. three more twin draws, one more glass on GL
	WwBl::zoneVk( 3 );
	WwBl::zoneWhy( "a blended rigid shape (the opaque route only)", true );
	gl( 1 );
	WwBl::glZoneSet( prev );
	// 7. outside the zone: an opaque GL draw with its own refusal must not reach the sorted tally
	WwBl::glZoneSet( WwBl::ZoneOpaque );
	WwBl::zoneWhy( "opaque only", true );
	gl( 5 );
	WwBl::glZoneSet( prev );

	const auto & s = z.whyN[WwBl::ZoneSorted];
	auto at = [&]( const char * k ) {
		auto it = s.find( k );
		return it == s.end() ? 0LL : it->second;
	};
	row( z.vk[WwBl::ZoneSorted] == 4, "sorted twin draws " + std::to_string( z.vk[WwBl::ZoneSorted] ) + " (want 4)" );
	row( z.draws[WwBl::ZoneSorted] == 6, "sorted GL draws " + std::to_string( z.draws[WwBl::ZoneSorted] ) + " (want 6)" );
	row( at( "a blended rigid shape (the opaque route only)" ) == 2, "glass refusals " +
		std::to_string( at( "a blended rigid shape (the opaque route only)" ) ) + " (want 2)" );
	row( at( "no twin: sky.prog" ) == 2, "no twin: sky.prog " + std::to_string( at( "no twin: sky.prog" ) ) + " (want 2)" );
	row( at( "no twin tried" ) == 1, "no twin tried " + std::to_string( at( "no twin tried" ) ) + " (want 1)" );
	row( at( "the RT key differs" ) == 1, "named over weak " + std::to_string( at( "the RT key differs" ) ) + " (want 1)" );
	row( s.size() == 4, "sorted reasons " + std::to_string( s.size() ) + " (want 4)" );
	long long sum = 0;
	for ( const auto & e : s )
		sum += e.second;
	row( sum == z.draws[WwBl::ZoneSorted], "reasons sum " + std::to_string( sum ) + " = GL draws" );
	row( z.whyN[WwBl::ZoneOpaque].size() == 1 && z.draws[WwBl::ZoneOpaque] == 5, "opaque kept apart" );
	const std::string line = WwBl::zoneLine( WwBl::ZoneSorted, "sorted", 2 );
	const std::string want = "vk sorted: drawn=4 gl=6 why: \"a blended rigid shape (the opaque route only)\" x2; \"no twin: sky.prog\" x2; +2 more";
	row( line == want, "line: " + line );
	WwBl::zoneReset();
	row( z.vk[WwBl::ZoneSorted] == 0 && z.whyN[WwBl::ZoneSorted].empty() && z.draws[WwBl::ZoneSorted] == 0 && z.why.empty(),
		"zoneReset clears the zone" );
	std::printf( "SORTEDVIEW %s (%d rows compared, %d failed)%s\n", fails ? "FAILED" : "OK", n, fails, red ? " RED stale-why" : "" );
	return fails ? 1 : 0;
}
