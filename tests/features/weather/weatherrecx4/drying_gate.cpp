// lane WEATHERRECX4: unit gate of the drying law (src/features/weather/airtempmath.h, Penman 1948 / Shuttleworth 1993).
// usage: drying_gate [red]   green: PASS; red (the law with humidity ignored): must FAIL.
// Bars: at the reference condition (15 C, RH 60 %, wind 2 m/s, clear sky, sun 45 deg) the time equals the file's;
// warmer, drier, windier, higher sun, clearer sky dry faster; saturated still night air clamps at 8x.
#include "features/weather/airtempmath.h"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace WwAirTemp;

static bool g_red = false;

static float factor( DryingAir a )
{
	if ( g_red )
		a.rh = -1.0f;	// the refuter: humidity does nothing
	return dryingFactorAir( a );
}

static DryingAir ref()
{
	DryingAir a;
	a.t = kDryRefT;
	a.rh = kDryRefRh;
	a.wind = kDryRefWind;
	a.sinSun = kDryRefSinSun;
	a.cover = kDryRefCover;
	return a;
}

static int fails = 0;
static void bar( bool ok, const char * what, float got )
{
	std::printf( "%s %-46s factor %.3f\n", ok ? "ok  " : "FAIL", what, double( got ) );
	if ( !ok )
		fails++;
}

int main( int argc, char ** argv )
{
	g_red = argc > 1 && !std::strcmp( argv[1], "red" );
	const float file = 90.0f;	// minutes (1.5 h)
	const float r = factor( ref() );
	bar( r == 1.0f && dryingMinutesAir( file, DryingAir() ) == file, "reference = the file's time (and all unset)", r );
	bar( factor( DryingAir() ) == 1.0f, "all inputs unset = reference", factor( DryingAir() ) );
	DryingAir a = ref();
	a.t = 30.0f;
	bar( factor( a ) < 1.0f, "30 C dries faster", factor( a ) );
	a = ref();
	a.t = 0.0f;
	bar( factor( a ) > 1.0f, "0 C dries slower", factor( a ) );
	a = ref();
	a.rh = 0.9f;
	bar( factor( a ) > 1.0f, "RH 90 % dries slower", factor( a ) );
	a = ref();
	a.rh = 0.2f;
	bar( factor( a ) < 1.0f, "RH 20 % dries faster", factor( a ) );
	a = ref();
	a.wind = 8.0f;
	bar( factor( a ) < 1.0f, "wind 8 m/s dries faster", factor( a ) );
	a = ref();
	a.wind = 0.0f;
	bar( factor( a ) > 1.0f, "still air dries slower", factor( a ) );
	a = ref();
	a.sinSun = 1.0f;
	bar( factor( a ) < 1.0f, "sun overhead dries faster", factor( a ) );
	a = ref();
	a.sinSun = -0.3f;
	bar( factor( a ) > 1.0f, "night dries slower", factor( a ) );
	a = ref();
	a.cover = 1.0f;
	bar( factor( a ) > 1.0f, "overcast dries slower", factor( a ) );
	a = ref();
	a.rh = 1.0f;
	a.sinSun = -0.3f;
	bar( factor( a ) == 8.0f, "saturated night air: clamp 8x", factor( a ) );
	a = ref();
	a.t = 40.0f;
	a.rh = 0.05f;
	a.wind = 15.0f;
	a.sinSun = 1.0f;
	bar( factor( a ) >= 0.25f, "desert noon stays >= 0.25x", factor( a ) );
	// the table for the hand-back (the file's 1.5 h at each condition)
	std::printf( "\ncondition                                  E mm/day  factor  hours (file 1.5)\n" );
	struct Row { const char * n; float t, rh, w, s, c; } rows[] = {
		{ "reference 15C 60% 2m/s clear sun45", 15, 0.6f, 2, 0.7071f, 0 },
		{ "same, sun overhead", 15, 0.6f, 2, 1, 0 },
		{ "same, overcast", 15, 0.6f, 2, 0.7071f, 1 },
		{ "same, night", 15, 0.6f, 2, -0.3f, 0 },
		{ "after rain 12C 95% 3m/s overcast noon", 12, 0.95f, 3, 0.9f, 1 },
		{ "hot dry 30C 25% 4m/s clear sun60", 30, 0.25f, 4, 0.866f, 0 },
		{ "cold 0C 80% 1m/s clear sun20", 0, 0.8f, 1, 0.342f, 0 },
		{ "night 10C 90% calm", 10, 0.9f, 0, -0.5f, 0.5f },
	};
	for ( const Row & w : rows ) {
		DryingAir x;
		x.t = w.t;
		x.rh = w.rh;
		x.wind = w.w;
		x.sinSun = w.s;
		x.cover = w.c;
		const float f = factor( x );
		std::printf( "%-42s %8.2f  %6.2f  %6.2f\n", w.n, penmanOf( x ), double( f ), 1.5 * double( f ) );
	}
	std::printf( "\n%s %s (%d bars failed)\n", fails ? "FAIL" : "PASS", g_red ? "red: humidity ignored" : "green", fails );
	return fails ? 1 : 0;
}
