// lane FIX1101A: the water quality preset's step table (src/features/water/waterpreset.h). Rules:
//  1 Low draws no water look (the game's water), Ultra every look as it ships (the default: no change)
//  2 a term on at a step stays on at every higher step (a step only adds)
//  3 the expensive terms sit high: the ocean spectrum from High, the glare facets and the contact foam at Ultra
//  4 parse: low|medium|high|ultra|0..3, any case; anything else = not set
// -DWATERPRESET_RED: today's code (no preset: every step lets every term) must FAIL rules 1 and 3.
//  5-6 (lane PERFFIX3) the Underwater fog at every step by default; red ultraonly = the rule before. -DUNDERFOG_RED
//      (the OffRow rule it shipped with) must FAIL rule 5.
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

// lane PERFFIX3: the Underwater fog row's verdict (waterpreset.h underFogOn); -DUNDERFOG_RED = the rule it shipped
// with before (an OffRow: its row, shipped OFF, at Ultra only) and must FAIL rule 5
static bool underT( int step, bool row, bool oldRow, bool red )
{
#ifdef UNDERFOG_RED
	( void ) oldRow;
	( void ) red;
	return row && lets( step, OffRow );
#else
	return underFogOn( step, row, oldRow, red );
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
#ifndef WATERPRESET_RED
	// rules 5-6 (PERFFIX3): the shipped values, kRows WW/CellUnderwater2 true (run.sh greps it) and the old key's OFF
	const bool rowShipped = true, oldShipped = false;
	bool every = true, redNone = true, redUltra = true;
	for ( int s = Low; s <= Ultra; s++ ) {
		every = every && underT( s, rowShipped, oldShipped, false );
		redNone = redNone && !underT( s, rowShipped, oldShipped, true );
		redUltra = redUltra && underT( s, rowShipped, true, true ) == ( s == Ultra );
	}
	check( every, "rule 5: the Underwater fog draws by default at Low, Medium, High and Ultra (the game always fogs)" );
	check( redNone && redUltra && !underT( Ultra, false, false, false ),
		"rule 6: red ultraonly = before (off by default; the old row ticked: Ultra only); the row unticked = off" );
#endif
	std::printf( "WATERPRESET TABLE %s%s (%d failed)\n", fails ? "FAIL" : "PASS",
#ifdef WATERPRESET_RED
		" [red]",
#else
		"",
#endif
		fails );
	return fails ? 1 : 0;
}
