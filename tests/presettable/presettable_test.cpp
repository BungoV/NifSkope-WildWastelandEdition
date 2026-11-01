// lane TIMING1 (bungo 11-01, "each feature per preset"): the built-in presets' table (src/ui/cellpresettable.h),
// every row x every preset, against its rules:
//  1. every code is one of 0 1 L M H U -, four per row, for plan and cols;
//  2. LOD is on at every preset (bungo 11-01);
//  3. a row ENTERS a column (on, or a level, other than its ship value) only for a ruling ('R'), a group master
//     ('G'), or when its plan has it there AND its measured GPU ms there meets its PERFRES target;
//  4. a ship-on row is never turned on at a level above its ship value without the same proof.
// Columns the plan wants that the gate keeps out are printed as OWED (not failures).
// Build: g++ -std=c++17 -I<tree>/src/ui presettable_test.cpp (tests/presettable/run.sh). Red: -DPRESETTABLE_RED, the
// old rule ("every Fusion Shaders row on at the preset's level, the heavy rows off at Low"): it must FAIL.
#include "cellpresettable.h"

#include <cstdio>
#include <cstring>

static int checks = 0, fails = 0;

static void check( bool ok, const char * what, const char * key, int p )
{
	checks++;
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s: %s%s%d\n", what, key, p >= 0 ? " preset " : "", p );
	}
}

static bool entered( char c )
{
	return c != '0' && c != '-';
}

static int rank( char c )	// a quality item's order; '1' (on, no level) above off
{
	const char * s = std::strchr( "01LMHU", c );
	return s ? int( s - "01LMHU" ) : -1;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	static const char * const kName[4] = { "Low", "Medium", "High", "Ultra" };
	int n = 0;
	const WwPresetRow * t = wwPresetTable( &n );
	std::printf( "preset table: %d rows%s\n", n,
#ifdef PRESETTABLE_RED
		" (RED: the old rule)"
#else
		""
#endif
	);
	std::printf( "%-46s ship plan  Low Med High Ultra basis\n", "row" );
	int owed = 0, farLod = 0;
	for ( int i = 0; i < n; i++ ) {
		const WwPresetRow & r = t[i];
		char got[4];
		for ( int p = 0; p < 4; p++ )
			got[p] = wwPresetCode( r.key, p );
		std::printf( "%-46s  %c   %.4s   %c   %c   %c    %c    %c\n", r.key, r.ship, r.plan, got[0], got[1], got[2], got[3], r.basis );
		check( std::strlen( r.plan ) == 4 && std::strlen( r.cols ) == 4, "four columns", r.key, -1 );
		check( std::strchr( "01LMHU-", r.ship ) != nullptr, "ship code", r.key, -1 );
		check( std::strchr( "MRG-", r.basis ) != nullptr, "basis", r.key, -1 );
		for ( int p = 0; p < 4; p++ ) {
			check( std::strchr( "01LMHU-", r.plan[p] ) && std::strchr( "01LMHU-", got[p] ), "codes", r.key, p );
			if ( r.basis == '-' )
				check( got[p] == '-' || got[p] == r.ship, "a row with no cost step is left or kept", r.key, p );
			if ( !entered( got[p] ) || got[p] == r.ship || r.basis == 'R' || r.basis == 'G' )
				continue;
			const bool planned = entered( r.plan[p] );
			const bool met = r.measured[p] >= 0.0f && r.target[p] >= 0.0f && r.measured[p] <= r.target[p];
			if ( !( planned && met ) )
				std::printf( "     %s %s: on without proof (plan %c, measured %.3f ms, target %.3f ms)\n", r.key, kName[p],
					r.plan[p], double( r.measured[p] ), double( r.target[p] ) );
			check( planned && met, "enters only planned + measured <= target", r.key, p );
			if ( entered( r.ship ) && rank( got[p] ) > rank( r.ship ) )
				check( met, "a level above the ship value needs the proof", r.key, p );
		}
		for ( int p = 0; p < 4; p++ )
			if ( entered( r.plan[p] ) && !entered( got[p] ) && !( entered( r.ship ) && got[p] == r.ship ) ) {
				owed++;
				std::printf( "     OWED %s %s: plan %c, measured %s, target %.3f ms\n", r.key, kName[p], r.plan[p],
					r.measured[p] < 0.0f ? "none" : "over", double( r.target[p] ) );
			}
		if ( std::strcmp( r.key, "CellWorkspaceFarLod" ) == 0 ) {
			farLod++;
			for ( int p = 0; p < 4; p++ )
				check( got[p] == '1', "LOD on at every preset (bungo 11-01)", r.key, p );
		}
		for ( int j = 0; j < i; j++ )
			check( std::strcmp( t[j].key, r.key ) != 0, "one row per key", r.key, -1 );
	}
	check( farLod == 1, "the LOD row is in the table", "CellWorkspaceFarLod", -1 );
	check( wwPresetCode( "NoSuchRow", 2 ) == '-', "a key the table lacks is left as it is", "NoSuchRow", 2 );
	std::printf( "%d columns OWED (the plan wants them; no measured ms meets the target yet)\n", owed );
	std::printf( "%d checks, %d failures\n", checks, fails );
	std::printf( fails ? "PRESETTABLE FAIL\n" : "PRESETTABLE PASS\n" );
	return fails ? 1 : 0;
}
