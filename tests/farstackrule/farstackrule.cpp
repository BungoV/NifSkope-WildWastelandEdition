/* tests/farstackrule (lane CELLUNI5, OPENREDS11's leftover): when the near cells refuse the far LOD bake's plugin
 * stack (src/cellfarstackrule.h, asked by cellFarLodStackFor in src/cellfarlod.cpp). The near gate's arm
 * (WW_CELL_FARLOD_NEARONLY=1) draws the far planes and nothing else, so it must keep the near cells' own stack; FARLOD3's
 * red (ownstack) still refuses; WW_CELL_FARLOD_RED=nearadopt is the red of the new rule (NEARONLY adopts, as before).
 * The red build (-DFARSTACKRULE_RED) runs main's rule (only ownstack refuses) and must fail.
 * Usage: farstackrule   exit 0 = all ok */
#include "cellfarstackrule.h"

#include <cstdio>

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

static CellFarStackRefusal rule( const char * red, int nearOnly )
{
#ifdef FARSTACKRULE_RED	// main before this lane: `wwEnv( "WW_CELL_FARLOD_RED" ) == "ownstack"` alone
	(void)nearOnly;
	return red && std::strcmp( red, "ownstack" ) == 0 ? CellFarStackRefusal::RedOwnStack : CellFarStackRefusal::Adopt;
#else
	return cellFarStackRefusal( red, nearOnly );
#endif
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	using R = CellFarStackRefusal;
	check( rule( nullptr, 0 ) == R::Adopt && rule( "", 0 ) == R::Adopt, "no switch: the bake's stack is adopted (FARLOD3)" );
	check( rule( "refusemissing", 0 ) == R::Adopt, "another FARLOD red alone does not refuse here" );
	check( rule( "ownstack", 0 ) == R::RedOwnStack && rule( "ownstack", 1 ) == R::RedOwnStack,
		"RED ownstack refuses, with or without NEARONLY (its own line)" );
	check( rule( nullptr, 1 ) == R::NearOnly && rule( "", 1 ) == R::NearOnly, "NEARONLY=1 refuses the bake's stack" );
	check( rule( "refusemissing", 1 ) == R::NearOnly, "NEARONLY refuses beside another FARLOD red" );
	check( rule( nullptr, 2 ) == R::NearOnly, "any non-zero NEARONLY refuses (wwEnvInt != 0, as its far-field arm reads it)" );
	check( rule( "nearadopt", 1 ) == R::Adopt && rule( "nearadopt", 0 ) == R::Adopt,
		"RED nearadopt: NEARONLY adopts as before this lane" );
	std::printf( "%s (%d fail)\n", fails ? "FARSTACKRULE FAIL" : "FARSTACKRULE PASS", fails );
	return fails ? 1 : 0;
}
