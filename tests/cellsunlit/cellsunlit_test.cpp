// lane INTSKY1: gl/cellsunlit.h, does an interior get the real sun + sky (on) and the live weather (weather)?
// CELLSUNLIT_RED=<check> breaks the rule that check guards; tests/cellsunlit/run.sh runs green + every red.
#include "gl/cellsunlit.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static int fails = 0;
static std::string red;

static void check( const char * name, bool ok )
{
	std::printf( "%s %s\n", ok ? "ok" : "FAIL", name );
	fails += ok ? 0 : 1;
}

using namespace wwsunlit;

static Decision D( bool flag, bool block, int en, int we, bool off )
{
	// the reds stand in for a broken rule: noflag ignores the flag, noblock ignores the file, weathersun ties weather to on
	if ( red == "noflag" )
		flag = false;
	if ( red == "noblock" )
		block = false;
	Decision d = decide( flag, block, en, we, off );
	if ( red == "weatherflag" && block && we == Absent )
		d.weather = flag;
	if ( red == "offall" && off )
		d.weather = false;
	return d;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( const char * r = std::getenv( "CELLSUNLIT_RED" ) )
		red = r;
	// vanilla: the flag decides both (ConcordMuseum01 0x1181; a closed cell 0x0001)
	Decision a = D( true, false, Absent, Absent, false ), b = D( false, false, Absent, Absent, false );
	check( "flag", a.on && a.weather && !b.on && !b.weather && !a.fromFile );
	// a file turns a closed cell on; weather missing = enabled
	Decision c = D( false, true, True, Absent, false );
	check( "block", c.on && c.weather && c.fromFile );
	check( "weatherfollows", c.weather == c.on && D( true, true, False, Absent, false ).weather == false );
	// a file turns a flagged cell off but keeps the weather
	Decision e = D( true, true, False, True, false );
	check( "offkeepsweather", !e.on && e.weather );
	// a bad enabled (Absent) keeps the flag's
	Decision f = D( true, true, Absent, False, false );
	check( "badkey", f.on && !f.weather );
	// the red off: no sun, the weather as decided (the batch's Use Sky Lighting look)
	Decision g = D( true, false, Absent, Absent, true );
	check( "redoff", !g.on && g.weather );
	std::printf( "%s: %d failed%s%s\n", fails ? "FAIL" : "PASS", fails, red.empty() ? "" : " under red ", red.c_str() );
	return fails ? 1 : 0;
}
