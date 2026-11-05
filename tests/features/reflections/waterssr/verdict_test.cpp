// lane FIX1101A: the SSR pass's verdict with the water's own part (src/features/reflections/cellssrwater.h). Rules:
//  1 the water's reflections run outdoors AND indoors with the SSR row off (every shot runner pins WW_CELL_SSR=0)
//  2 the water's own row off (WW_CELL_SSR_WATER=0) stops them; the opaque surfaces keep the SSR row's verdict
//  3 with no water nothing changes: the SSR row and SSR outdoors decide exactly as before (verdicts 1, 2, 4)
//  4 the red (WW_CELL_SSR_WATER_RED=1) gives the old verdict on every input (the table of the old expression)
// Build: g++ -std=c++17 -I src/gl tests/features/reflections/waterssr/verdict_test.cpp; -DWATERSSR_RED runs every case with the red set and
// must FAIL rule 1 (the old coupling: no water reflection with the SSR row off).
#include "cellssrwater.h"

#include <cstdio>

static int fails = 0;

static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	fails += ok ? 0 : 1;
}

// the old cellssr.cpp expression (main 9b443dcf), for rule 4
static int oldVerdict( const WwSsrVerdictIn & v )
{
	int verdict = !v.cell ? 0 : !v.rowOn ? 1 : ( !v.interior && !v.ext && !v.waterWanted ) ? 2 : !v.aoTargets ? 3 : 4;
	if ( verdict == 4 && !v.interior && !v.ext )
		verdict = 5;
	return verdict;
}

#ifdef WATERSSR_RED
static const bool kRed = true;
#else
static const bool kRed = false;
#endif

static WwSsrVerdictIn in( bool rowOn, bool ext, bool interior, bool water, bool waterRow = true )
{
	WwSsrVerdictIn v;
	v.cell = true;
	v.rowOn = rowOn;
	v.ext = ext;
	v.interior = interior;
	v.waterWanted = water;
	v.waterRow = waterRow;
	v.red = kRed;
	v.aoTargets = true;
	return v;
}

int main()
{
	// rule 1
	const WwSsrVerdictIn outOff = in( false, false, false, true ), inOff = in( false, false, true, true );
	check( wwSsrWaterPart( outOff ) && wwSsrVerdict( outOff ) == 6 && wwSsrWantsTargets( outOff ) && wwSsrWaterOnly( outOff ),
		"rule 1: exterior, SSR row off (the runners' pin): the water's reflections run (verdict 6, water only)" );
	check( wwSsrWaterPart( inOff ) && wwSsrVerdict( inOff ) == 6 && wwSsrWaterOnly( inOff ),
		"rule 1: interior, SSR row off: the water's reflections run (verdict 6, water only)" );
	const WwSsrVerdictIn outOn = in( true, false, false, true );
	check( wwSsrVerdict( outOn ) == 5 && wwSsrWaterOnly( outOn ), "rule 1: exterior, SSR row on, outdoors off: verdict 5 as before" );
	// rule 2
	const WwSsrVerdictIn noRow = in( false, false, false, true, false ), noRowIn = in( true, false, true, true, false );
	check( kRed || ( !wwSsrWaterPart( noRow ) && wwSsrVerdict( noRow ) == 1 && !wwSsrWantsTargets( noRow ) ),
		"rule 2: the water's row off, SSR row off: no pass (verdict 1)" );
	check( kRed || ( !wwSsrWaterPart( noRowIn ) && wwSsrVerdict( noRowIn ) == 4 && !wwSsrWaterOnly( noRowIn ) ),
		"rule 2: the water's row off indoors with the SSR row on: the opaque surfaces still reflect (verdict 4)" );
	// rule 3 + 4: every input combination
	int same3 = 0, n3 = 0, same4 = 0, n4 = 0;
	for ( int m = 0; m < 64; m++ ) {
		WwSsrVerdictIn v;
		v.cell = m & 1;
		v.rowOn = m & 2;
		v.ext = m & 4;
		v.interior = m & 8;
		v.waterWanted = m & 16;
		v.aoTargets = m & 32;
		v.waterRow = true;
		v.red = kRed;
		if ( !v.waterWanted ) {
			n3++;
			same3 += wwSsrVerdict( v ) == oldVerdict( v ) ? 1 : 0;
		}
		v.red = true;
		n4++;
		same4 += wwSsrVerdict( v ) == oldVerdict( v ) && wwSsrWantsTargets( v ) == ( v.cell && v.rowOn && ( v.interior || v.ext || v.waterWanted ) )
			? 1 : 0;
	}
	char b[160];
	std::snprintf( b, sizeof( b ), "rule 3: no water: the old verdict on %d of %d inputs", same3, n3 );
	check( same3 == n3, b );
	std::snprintf( b, sizeof( b ), "rule 4: the red: the old verdict and targets on %d of %d inputs", same4, n4 );
	check( same4 == n4, b );
	std::printf( "WATERSSR VERDICT %s%s (%d failed)\n", fails ? "FAIL" : "PASS", kRed ? " [red]" : "", fails );
	return fails ? 1 : 0;
}
