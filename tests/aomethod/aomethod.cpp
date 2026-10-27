// lane SSAO2: src/gl/cellaomethod.h alone (no Qt, no GL).
// g++ -std=c++17 -I<tree>/src tests/aomethod/aomethod.cpp; -DAOMETHOD_RED tests the old SSR coupling (must fail)
#include "gl/cellaomethod.h"

#include <cstdio>

static int fails = 0;

static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

#ifdef AOMETHOD_RED
// the coupling before lane AOOFF1, which cellssr.h still described: the reflections need the AO row
static bool keep( bool aoOn, bool forSsr, bool forOthers ) { (void)forSsr; return aoOn || forOthers; }
#else
static bool keep( bool aoOn, bool forSsr, bool forOthers ) { return wwAoSharedPassRuns( aoOn, forSsr, forOthers, false ); }
#endif

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// the shared pass: AO off drops only the AO term
	check( keep( false, true, false ), "AO off + SSR on: the opaque pass and depth pyramid run" );
	check( keep( true, false, false ), "AO on: the pass runs" );
	check( keep( true, true, false ), "AO on + SSR on: the pass runs" );
	check( keep( false, false, true ), "AO off + decal/contact/GI: the pass runs" );
	check( !keep( false, false, false ), "AO off, nothing reads it: no pass" );
	check( !wwAoSharedPassRuns( false, true, false, true ), "red old: AO off + SSR on gets no pass" );
	check( wwAoSharedPassRuns( true, true, false, true ), "red old: AO on still runs it" );
	// the method names
	check( wwAoMethodParse( "vanilla" ) == WwAoVanilla, "parse vanilla" );
	check( wwAoMethodParse( "gtao" ) == WwAoGtao, "parse gtao" );
	check( wwAoMethodParse( "xegtao" ) == WwAoXeGtao, "parse xegtao" );
	check( wwAoMethodParse( "hbao" ) == WwAoHbao, "parse hbao" );
	check( wwAoMethodParse( "" ) == -1 && wwAoMethodParse( "ssao" ) == -1 && wwAoMethodParse( nullptr ) == -1, "parse others -1" );
	for ( int m = 0; m < WwAoMethodCount; m++ )
		check( wwAoMethodParse( wwAoMethodName( m ) ) == m, "name round trip" );
	// the stage lists (FO4CS: GTAO/HBAO+ two denoise passes, XeGTAO its own denoise then one)
	const int * s = wwAoMethodStages( WwAoGtao );
	check( s[0] == 3 && s[1] == 5 && s[2] == 5 && s[3] == 0, "gtao stages 3 5 5" );
	s = wwAoMethodStages( WwAoHbao );
	check( s[0] == 4 && s[1] == 5 && s[2] == 5 && s[3] == 0, "hbao stages 4 5 5" );
	s = wwAoMethodStages( WwAoXeGtao );
	check( s[0] == 6 && s[1] == 7 && s[2] == 5 && s[3] == 0, "xegtao stages 6 7 5" );
	check( wwAoMethodStages( WwAoVanilla )[0] == 0, "vanilla: no cell_aox stage" );
	std::printf( "AOMETHOD %s %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
