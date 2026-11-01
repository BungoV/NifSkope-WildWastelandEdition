// lane FIX1101A lodch r2 (LATE ITEM 25): the far objects' ground contact (src/lodchsky.h contactAo), the .lodi v12
// ramp folded into the AO byte the instanced rows carry. One rule a check:
//   foot     a wall foot AT the terrain whose AO reads open (the 8-ray cast misses grazing ground) draws <= 0.70
//   nodouble where the AO already holds the contact (a foot at 0.40) the result is the AO, never darker
//   air      ramp 0 (256 u or more above the terrain) leaves every AO as it is
//   floor    nothing the AO left above 0.65 goes below 0.65 (not too dark: AO2's tuning keeps open walls >= 0.6)
//   monotone a vertex nearer the terrain is never lighter
// Green prints "LODCHGROUND PASS" (rc 0). -DLODCH_GROUND_RED=1 (today: the ramp not drawn) and =2 (the product, which
// darkens twice) must print "LODCHGROUND FAIL" (rc 1).
#include "lodchsky.h"

#include <cstdio>

int main()
{
	int fails = 0;
	auto check = [&]( const char * name, bool ok, const char * fmt, double a, double b ) {
		std::printf( "%-9s %s  ", name, ok ? "ok  " : "FAIL" );
		std::printf( fmt, a, b );
		std::printf( "\n" );
		if ( !ok )
			fails++;
	};
	const float foot = LodchSky::contactAo( 1.0f, 255 );
	check( "foot", foot <= 0.70f, "open AO at the terrain -> %.3f (bar <= %.2f)", foot, 0.70 );
	const float dbl = LodchSky::contactAo( 0.40f, 255 );
	check( "nodouble", dbl > 0.40f - 1e-6f && dbl < 0.40f + 1e-6f, "AO 0.40 at the terrain -> %.3f (bar = %.2f)", dbl, 0.40 );
	float worstAir = 0.0f;
	for ( float a : { 0.1f, 0.3f, 0.65f, 0.8f, 1.0f } )
		worstAir = std::max( worstAir, std::fabs( LodchSky::contactAo( a, 0 ) - a ) );
	check( "air", worstAir < 1e-6f, "ramp 0: worst |change| %.6f (bar %.0f)", worstAir, 0.0 );
	float lowest = 1.0f;
	for ( int r = 0; r <= 255; r++ )
		for ( float a = 0.65f; a <= 1.0f; a += 0.01f )
			lowest = std::min( lowest, LodchSky::contactAo( a, r ) );
	check( "floor", lowest >= 0.65f - 1e-4f, "AO >= 0.65, every ramp: lowest %.3f (bar >= %.2f)", lowest, 0.65 );
	bool mono = true;
	for ( float a : { 0.2f, 0.5f, 0.9f, 1.0f } )
		for ( int r = 1; r <= 255; r++ )
			if ( LodchSky::contactAo( a, r ) > LodchSky::contactAo( a, r - 1 ) + 1e-6f )
				mono = false;
	check( "monotone", mono, "nearer the terrain never lighter: %.0f (bar %.0f)", mono ? 1.0 : 0.0, 1.0 );
	std::printf( fails ? "LODCHGROUND FAIL (%d)\n" : "LODCHGROUND PASS\n", fails );
	return fails ? 1 : 0;
}
