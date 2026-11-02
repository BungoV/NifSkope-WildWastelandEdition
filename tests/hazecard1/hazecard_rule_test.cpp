/* lane HAZECARD1: unit gate for src/gl/fxroomrule.h (which exterior lit effect scales its base by the room share).
 * Build: g++ -std=c++17 -I<tree>/src tests/hazecard1/hazecard_rule_test.cpp -o hc.exe  (exit 0 = PASS)
 * Red:   add -DHC_RED: the rule under test becomes the one before the lane (every exterior lit effect room-lit, soft
 *        cards too), and the gate must FAIL. */
#include "gl/fxroomrule.h"

#include <cstdio>

#ifdef HC_RED
static bool ruleUnderTest( bool exterior, bool fxLit, bool, int red ) { return !( red & 1 ) && exterior && fxLit; }
#else
static bool ruleUnderTest( bool exterior, bool fxLit, bool soft, int red ) { return wwFxRoomLitRule( exterior, fxLit, soft, red ); }
#endif

int main()
{
	struct Case
	{
		const char * what;
		bool exterior, fxLit, soft;
		int red;
		bool want;
	};
	const Case cases[] = {
		// the game: a lit effect outdoors takes the weather's Effect Lighting as its base, whatever stands round it
		{ "haze card outdoors (soft, lit): the game's base", true, true, true, 0, false },
		// GIDARK1 kept for the rest: the lens in an open shop follows the room
		{ "lens outdoors (not soft, lit): the room share", true, true, false, 0, true },
		{ "interior lit card: never room-lit", false, true, true, 0, false },
		{ "interior lens: never room-lit", false, true, false, 0, false },
		{ "unlit effect outdoors: never room-lit", true, false, true, 0, false },
		// reds
		{ "red HAZECARD room: the soft card room-lit as before", true, true, true, 2, true },
		{ "red FXROOM game: the lens takes the game's base", true, true, false, 1, false },
		{ "red FXROOM game wins over HAZECARD room", true, true, true, 3, false },
	};
	int fails = 0;
	for ( const Case & c : cases ) {
		const bool got = ruleUnderTest( c.exterior, c.fxLit, c.soft, c.red );
		if ( got != c.want ) {
			fails++;
			std::printf( "FAIL %s: got %d want %d\n", c.what, int( got ), int( c.want ) );
		}
	}
	std::printf( "%s: %d of %d cases\n", fails ? "FAIL" : "PASS", int( sizeof( cases ) / sizeof( cases[0] ) ) - fails,
		int( sizeof( cases ) / sizeof( cases[0] ) ) );
	return fails ? 1 : 0;
}
