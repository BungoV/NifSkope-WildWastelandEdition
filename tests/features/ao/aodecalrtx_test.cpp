// PERFFIX2 (2026-11-01): src/cellaodecalrule.h, when the baked AO decals are built and drawn.
//   g++ -std=c++17 -I src tests/features/ao/aodecalrtx_test.cpp -o x && ./x      -> "AODECALRTX PASS 3", exit 0
//   ... -DAODECALRTX_RED (the rule before PERFFIX2: the row alone)                -> "FAIL rtx", exit 1
#include "features/ao/cellaodecalrule.h"
#include <cstdio>

#ifdef AODECALRTX_RED
#define aoDecalWorksRule( row, prt, red ) ( row )
#endif

static int g_fail = 0, g_pass = 0;
static void report( const char * name, bool ok, const char * what )
{
	std::printf( "%s %s: %s\n", ok ? "PASS" : "FAIL", name, what );
	( ok ? g_pass : g_fail )++;
}

int main()
{
	// the row off: never, whatever the GI
	report( "off", !aoDecalWorksRule( false, true, false ) && !aoDecalWorksRule( false, false, true ), "row off: no decals" );
	// the old PRT draws: the decals scale its sky term (AODECAL1's design, unchanged)
	report( "prt", aoDecalWorksRule( true, true, false ), "row on + PRT: built and drawn" );
	// the ray-traced volume alone (the default since PRTOFF1): the volume traces the copies, nothing reads a decal;
	// the red builds them anyway
	report( "rtx", !aoDecalWorksRule( true, false, false ) && aoDecalWorksRule( true, false, true ),
		"row on, no PRT: none (the red: built)" );
	std::printf( "AODECALRTX %s %d\n", g_fail ? "FAIL" : "PASS", g_fail ? g_fail : g_pass );
	return g_fail ? 1 : 0;
}
