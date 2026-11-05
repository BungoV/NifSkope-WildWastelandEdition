/* lane FO76WEATHER1: unit test of src/features/weather/rainwetlaw.h (Fallout 76's rain wetness law, Fallout 4's clock).
 * Build: g++ -std=c++17 -I<tree>/src tests/features/weather/rainwet/rainwet.cpp. Red: -DRAINWET_RED = the base (shore wetness only:
 * the rain wets nothing), which must fail. Hand values from the law in the header's comment, worked by hand. */
#include "features/weather/rainwetlaw.h"
#include "features/far-ground/landlodspec.h"

#include <cmath>
#include <cstdio>

namespace
{
int fails = 0, checks = 0;

void check( const char * what, double got, double want, double tol = 1e-5 )
{
	checks++;
	const bool ok = std::fabs( got - want ) <= tol;
	if ( !ok )
		fails++;
	std::printf( "%s %-44s got %.6f want %.6f\n", ok ? "ok  " : "FAIL", what, got, want );
}

// the pixel as drawn: albedo factor and new smoothness at amount a (the red: the base draws no rain wetness)
void pixel( float a, float smooth, float f0, float & albedoK, float & smoothOut, float custom = 0.0f, float use = 0.0f )
{
#ifdef RAINWET_RED
	(void)a; (void)f0; (void)custom; (void)use;
	albedoK = 1.0f;
	smoothOut = smooth;
#else
	using namespace WwRainWet;
	const float w = wetness( a, 1.0f, porosity( smooth, custom, use ), metalMask( f0, f0, f0 ) );
	albedoK = WwRainWet::albedoK( w );
	smoothOut = wetSmooth( smooth, w );
#endif
}
}	// namespace

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	float k = 0.0f, s = 0.0f;
	// dry: the old values (the shaders skip the block at amount 0, so the dry frame is bit for bit the old one)
	pixel( 0.0f, 0.3f, 0.04f, k, s );
	check( "dry albedo x1", k, 1.0 );
	check( "dry smoothness kept", s, double( 0.3f ), 1e-6 );
	// porosity: 1 at 0.1, 0 at 0.7, linear between
	check( "porosity at smooth 0.1", WwRainWet::porosity( 0.1f ), 1.0 );
	check( "porosity at smooth 0.4", WwRainWet::porosity( 0.4f ), 1.1666 - 0.6664 );
	check( "porosity at smooth 0.7", WwRainWet::porosity( 0.7f ), 1.1666 - 1.1662 );
	check( "porosity at smooth 0.75", WwRainWet::porosity( 0.75f ), 0.0 );
	check( "custom porosity wins", WwRainWet::porosity( 0.9f, 0.6f, 1.0f ), 0.6 );
	// full rain on rough stone (smooth 0.1, dielectric)
	pixel( 1.0f, 0.1f, 0.04f, k, s );
	check( "rain: rough albedo x0.2", k, 0.2 );
	check( "rain: rough smoothness 0.46", s, 1.0 - 0.6 * 0.9 );
	// half rain, smooth 0.4
	pixel( 0.5f, 0.4f, 0.04f, k, s );
	const double w = 0.5 * ( 1.1666 - 0.6664 );
	check( "half rain: smooth 0.4 albedo", k, 1.0 - 0.8 * w );
	check( "half rain: smooth 0.4 smoothness", s, 1.0 - ( 1.0 - 0.4 * w ) * 0.6 );
	// glossy and metal never wet
	pixel( 1.0f, 0.8f, 0.04f, k, s );
	check( "rain: glossy 0.8 unchanged", k, 1.0 );
	pixel( 1.0f, 0.1f, 0.9f, k, s );
	check( "rain: metal (F0 0.9) unchanged", k, 1.0 );
	check( "metal mask at F0 0.5", WwRainWet::metalMask( 0.5f, 0.5f, 0.5f ), 0.0 );
	check( "metal mask at F0 0.51", WwRainWet::metalMask( 0.51f, 0.51f, 0.51f ), 1.0 );
	// the clock: toward 1 or 0 it is Fallout 4's wetStep exactly
	float a = 0.0f, b = 0.0f;
	for ( int i = 0; i < 40; i++ ) {
		a = WwRainWet::step( a, 1.0f, 1.0f );
		b = LandLodSpec::wetStep( b, true, 1.0f );
	}
	check( "clock wets like wetStep (40 s)", a, b, 0.0 );
	check( "clock 40 s of rain", a, 1.0 );
	for ( int i = 0; i < 10; i++ ) {
		a = WwRainWet::step( a, 0.0f, 1.0f );
		b = LandLodSpec::wetStep( b, false, 1.0f );
	}
	check( "clock dries like wetStep (10 s)", a, b, 0.0 );
	check( "clock 10 s dry", a, 0.8 );
	check( "clock stops at a half share", WwRainWet::step( 0.49f, 0.5f, 1.0f ), 0.5 );
	std::printf( "%d of %d failed\n", fails, checks );
	return fails ? 1 : 0;
}
