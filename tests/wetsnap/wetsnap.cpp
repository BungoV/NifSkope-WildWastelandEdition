/* lane WETSNAP1: unit test of the wetness snap (src/gl/rainwetlaw.h update, src/features/far-ground/landlodspec.h wetUpdate): Fallout 4's
 * wetness update dries at fDrySpeed only while a rain weather is one of the change's two weathers, and puts the
 * wetness to 0 at once indoors or when neither weather rains.
 * Build: g++ -std=c++17 -I<tree>/src tests/wetsnap/wetsnap.cpp. Red: -DWETSNAP_RED = the viewer's switch off
 * (WW_CELL_RAINWET_RED=nosnap, the FO76WEATHER2 slow dry), which must fail. Hand values worked by hand. */
#include "gl/rainwetlaw.h"
#include "features/far-ground/landlodspec.h"

#include <cmath>
#include <cstdio>

namespace
{
int fails = 0, checks = 0;

#ifdef WETSNAP_RED
constexpr bool kSnap = false;
#else
constexpr bool kSnap = true;
#endif

void check( const char * what, double got, double want, double tol = 1e-6 )
{
	checks++;
	const bool ok = std::fabs( got - want ) <= tol;
	if ( !ok )
		fails++;
	std::printf( "%s %-52s got %.6f want %.6f\n", ok ? "ok  " : "FAIL", what, got, want );
}

float up( float wet, float target, float dt, bool now, bool prev, bool ext )
{
	return WwRainWet::update( wet, target, dt, now, prev, ext, kSnap );
}
}	// namespace

int main()
{
	// rain falls: no snap, the clock as before
	check( "rain, no change: wets 1 s", up( 0.5f, 1.0f, 1.0f, true, false, true ), 0.53 );
	check( "clear -> rain change: wets toward its share", up( 0.1f, 0.3f, 1.0f, true, false, true ), 0.13 );
	// rain -> clear change still running: the leaving weather rains, so it dries slowly
	check( "rain -> clear, change running: dries 1 s", up( 0.6f, 0.0f, 1.0f, false, true, true ), 0.58 );
	// the change has ended (no leaving weather) or the weather never rained: 0 at once
	check( "rain -> clear, change over: snaps", up( 0.58f, 0.0f, 1.0f, false, false, true ), 0.0 );
	check( "clear, no change: snaps", up( 0.9f, 0.0f, 0.016f, false, false, true ), 0.0 );
	check( "snow weather (no rain): snaps", up( 0.4f, 0.0f, 0.016f, false, false, true ), 0.0 );
	check( "indoors: snaps", up( 0.5f, 0.0f, 0.016f, false, false, false ), 0.0 );
	check( "indoors while rain outside: snaps", up( 0.5f, 0.0f, 0.016f, true, false, false ), 0.0 );
	check( "snaps(): rain now outdoors", WwRainWet::snaps( true, false, true ) ? 1.0 : 0.0, 0.0 );
	check( "snaps(): rain leaving outdoors", WwRainWet::snaps( false, true, true ) ? 1.0 : 0.0, 0.0 );
	check( "snaps(): no rain outdoors", WwRainWet::snaps( false, false, true ) ? 1.0 : 0.0, 1.0 );
	check( "snaps(): indoors", WwRainWet::snaps( true, true, false ) ? 1.0 : 0.0, 1.0 );

	// switch off = today's bytes: update with snapOn false is step, bit for bit, over a grid
	int same = 0, n = 0;
	for ( int a = 0; a <= 20; a++ )
		for ( int t = 0; t <= 4; t++ )
			for ( int m = 0; m < 8; m++ ) {
				const float w = float( a ) * 0.05f, tg = float( t ) * 0.25f, dt = 0.016f + float( m ) * 0.7f;
				const float x = WwRainWet::update( w, tg, dt, m & 1, m & 2, m & 4, false );
				const float y = WwRainWet::step( w, tg, dt );
				same += ( x == y ) ? 1 : 0;
				n++;
			}
	check( "snap off == step (bit for bit, 840 cases)", double( same ), double( n ), 0.0 );

	// a whole run, target 1 or 0: the viewer's law is the game's (landlodspec.h wetUpdate) exactly
	float v = 0.0f, g = 0.0f;
	int apart = 0;
	for ( int s = 0; s < 90; s++ ) {
		const bool now = s < 40, prev = s >= 40 && s < 60, ext = s < 85;	// rain 40 s, a 20 s change, clear, indoors
		v = up( v, now ? 1.0f : 0.0f, 1.0f, now, prev, ext );
		g = LandLodSpec::wetUpdate( g, now, prev, ext, 1.0f );
		apart += ( v != g ) ? 1 : 0;
		if ( s == 39 )
			check( "run: 40 s of rain", v, 1.0 );
		if ( s == 59 )
			check( "run: end of a 20 s change (dried 0.4)", v, 0.6, 1e-5 );
		if ( s == 60 )
			check( "run: first clear second (snapped)", v, 0.0 );
	}
	check( "run: viewer == game wetUpdate (90 steps)", double( apart ), 0.0, 0.0 );
	std::printf( "%d of %d failed\n", fails, checks );
	return fails ? 1 : 0;
}
