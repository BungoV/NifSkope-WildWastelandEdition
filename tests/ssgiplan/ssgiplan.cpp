// lane PERFFIX2 (item 3): src/gl/cellssgiplan.h alone (no Qt, no GL).
// g++ -std=c++17 -I<tree>/src tests/ssgiplan/ssgiplan.cpp; -DSSGIPLAN_RED=3a tests the pass before the sub-timers
// (one zone, no stage groups): must fail.
#include "gl/cellssgiplan.h"

#include <cstdio>
#include <cstring>

static int fails = 0;

static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

static int subOf( int stage, bool emissive )
{
#if defined( SSGIPLAN_RED ) && SSGIPLAN_RED == 3
	( void ) stage; ( void ) emissive;
	return -1;	// before 3a: the ssgi zone alone, no stage is timed apart
#else
	return wwSsgiStageSub( stage, emissive );
#endif
}

static int emissiveOf( int red )
{
#if defined( SSGIPLAN_RED ) && SSGIPLAN_RED == 4
	return ( red & 128 ) ? WwSsgiEmitFolded : WwSsgiEmitChain;	// before 3b: the emissive chain whenever not `noemissive`
#else
	return wwSsgiEmissive( red );
#endif
}

static bool wantsTarget( int red )
{
#if defined( SSGIPLAN_RED ) && SSGIPLAN_RED == 4
	return !( red & 128 );
#else
	return wwSsgiWantsEmissionTarget( red );
#endif
}

static bool hizBuilt( int red )
{
#if defined( SSGIPLAN_RED ) && SSGIPLAN_RED == 5
	return !( red & 32 );	// before 3c: the full-size interval pyramid every frame unless `nohiz`
#else
	return wwSsgiHizBuilt( red );
#endif
}

static WwSsgiTier tierOf( int q, int red )
{
#if defined( SSGIPLAN_RED ) && SSGIPLAN_RED == 6
	return wwSsgiTier( q, red | 4096 );	// before 3d: FO4CS's tiers at every setting
#else
	return wwSsgiTier( q, red );
#endif
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// 3a: every stage the pass draws lands in one named group; the groups split the cast from its denoise
	const int stages[] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 11 };
	bool all = true;
	for ( int s : stages )
		for ( int e = 0; e < 2; e++ ) {
			if ( s == 8 && !e )
				continue;
			const int g = subOf( s, e != 0 );
			all = all && g >= 0 && g < WwSsgiSubCount && std::strncmp( wwSsgiSubName( g ), "ssgi-", 5 ) == 0;
		}
	check( all, "3a: every drawn stage has a sub-timer group with an ssgi- name" );
	check( subOf( 1, false ) == WwSsgiSubCast && subOf( 2, false ) == WwSsgiSubTemporal, "3a: cast and temporal apart" );
	check( subOf( 4, false ) == WwSsgiSubDenoise && subOf( 7, false ) == WwSsgiSubDenoise, "3a: robust + pyramid = denoise" );
	check( subOf( 8, true ) == WwSsgiSubEcast && subOf( 2, true ) == WwSsgiSubEchain && subOf( 7, true ) == WwSsgiSubEchain,
		"3a: the emissive cast and its chain apart from the ordinary ones" );
	check( subOf( 10, false ) == WwSsgiSubHiz && subOf( 11, false ) == WwSsgiSubHiz, "3a: the interval pyramid" );
	check( subOf( 0, false ) == -1 && subOf( 9, false ) == -1, "3a: no stage 0 / 9" );
	int names = 0;
	for ( int g = 0; g < WwSsgiSubCount; g++ )
		for ( int h = g + 1; h < WwSsgiSubCount; h++ )
			names += std::strcmp( wwSsgiSubName( g ), wwSsgiSubName( h ) ) == 0;
	check( names == 0, "3a: the group names are distinct" );
	// 3b: the emitters fold into the ordinary march by default; the separate chain is the red
	check( emissiveOf( 0 ) == WwSsgiEmitFolded, "3b: default: folded (one cast, one denoise)" );
	check( !wantsTarget( 0 ), "3b: default: the scene pass writes no emission target" );
	check( emissiveOf( 1024 ) == WwSsgiEmitChain && wantsTarget( 1024 ), "3b: red emitchain: stage 8's chain + its target" );
	check( emissiveOf( 128 ) == WwSsgiEmitFolded && !wantsTarget( 128 ) && emissiveOf( 128 | 1024 ) == WwSsgiEmitFolded,
		"3b: noemissive wins over emitchain (no target)" );
	check( emissiveOf( 32 | 64 ) == WwSsgiEmitFolded, "3b: other reds keep the fold" );
	// 3c: no full-size pyramid by default; the exact early out stays; nohiz keeps SSGI2's meaning (neither)
	check( !hizBuilt( 0 ) && wwSsgiFullBreak( 0 ), "3c: default: no interval pyramid, full-mask early out on" );
	check( hizBuilt( 2048 ), "3c: red hiz: the pyramid again" );
	check( !hizBuilt( 32 ) && !hizBuilt( 32 | 2048 ) && !wwSsgiFullBreak( 32 ), "3c: nohiz: no pyramid, no early out" );
	// 3d: the tiers step the cost (taps per frame pixel rise), 12 steps each (the bounce's energy), no full-size
	// tier (its robust denoise alone, 1.9 ms offline, is over Ultra's 1.76 target), FO4CS's set under `oldtiers`
	bool steps12 = true, notFull = true, taps = true, rising = true;
	double prev = 0.0;
	for ( int q = 0; q < 4; q++ ) {
		const WwSsgiTier t = tierOf( q, 0 );
		steps12 = steps12 && t.steps == 12;
		notFull = notFull && t.div >= 2;
		taps = taps && t.robustTaps >= 1 && t.robustTaps <= 16 && t.slices >= 2;
		const double perPx = double( t.slices * t.steps ) / double( t.div * t.div ) + double( t.robustTaps ) / double( t.div * t.div ) * 0.25;
		rising = rising && perPx > prev;
		prev = perPx;
	}
	check( steps12, "3d: 12 steps a side on every tier" );
	check( notFull, "3d: no full-size tier" );
	check( taps, "3d: 2+ slices, 1..16 robust taps" );
	check( rising, "3d: each tier costs more per frame pixel than the one below" );
	const WwSsgiTier o = wwSsgiTier( 3, 4096 );
	check( o.div == 1 && o.slices == 4 && o.steps == 8 && o.robustTaps == 16, "3d: red oldtiers: FO4CS's Ultra (full 4x8)" );
	check( wwSsgiTier( -3, 0 ).div == wwSsgiTier( 0, 0 ).div && wwSsgiTier( 9, 0 ).div == wwSsgiTier( 3, 0 ).div, "3d: clamped quality" );
	check( wwSsgiWorkDim( 1920, 4 ) == 480 && wwSsgiWorkDim( 1027, 4 ) == 257 && wwSsgiWorkDim( 1027, 2 ) == 514
		&& wwSsgiWorkDim( 1027, 1 ) == 1027, "3d: work size rounds up" );
	std::printf( "SSGIPLAN %s %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
