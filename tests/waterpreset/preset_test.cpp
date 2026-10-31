// lane FIX1101A: the water quality preset's step table (src/gl/waterpreset.h). Rules:
//  1 Low draws no water look (the game's water), Ultra every look as it ships (the default: no change)
//  2 a term on at a step stays on at every higher step (a step only adds)
//  3 the expensive terms sit high: the ocean spectrum from High, the glare facets and the contact foam at Ultra
//  4 parse: low|medium|high|ultra|0..3, any case; anything else = not set
// -DWATERPRESET_RED: today's code (no preset: every step lets every term) must FAIL rules 1 and 3.
#include "waterpreset.h"

#include <cstdio>

using namespace WwWaterPreset;
static int fails = 0;

static bool letsT( int step, Term t )
{
#ifdef WATERPRESET_RED
	( void ) step;
	( void ) t;
	return true;
#else
	return lets( step, t );
#endif
}

static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

int main()
{
	const Term water[8] = { Clear, Refl, SoftEdge, ShoreFoam, RiverFoam, Ocean, Glint, Contact };
	bool lowNone = true, ultraAll = true, mono = true;
	for ( Term t : water ) {
		lowNone = lowNone && !letsT( Low, t );
		ultraAll = ultraAll && letsT( Ultra, t );
		for ( int s = 0; s < 3; s++ )
			mono = mono && ( !letsT( s, t ) || letsT( s + 1, t ) );
	}
	check( lowNone && letsT( Low, NotWater ), "rule 1: Low draws no water look (other looks untouched)" );
	check( ultraAll && letsT( Ultra, OffRow ), "rule 1: Ultra lets every water look (and the OFF rows when turned on)" );
	check( mono, "rule 2: a step only adds terms" );
	check( !letsT( Medium, Ocean ) && letsT( High, Ocean ) && !letsT( High, Glint ) && !letsT( High, Contact )
		&& letsT( Medium, Clear ) && letsT( Medium, Refl ), "rule 3: ocean from High, glare + contact at Ultra, colour + reflections from Medium" );
	check( parse( "low" ) == Low && parse( "Medium" ) == Medium && parse( "2" ) == High && parse( "ULTRA" ) == Ultra
		&& parse( "" ) == -1 && parse( "max" ) == -1, "rule 4: parse" );
	std::printf( "WATERPRESET TABLE %s%s (%d failed)\n", fails ? "FAIL" : "PASS",
#ifdef WATERPRESET_RED
		" [red]",
#else
		"",
#endif
		fails );
	return fails ? 1 : 0;
}
