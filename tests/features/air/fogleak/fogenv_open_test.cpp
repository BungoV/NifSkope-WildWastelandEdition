// Lane FOGENV1: rtddgimath.h openIrradiance, the open field the air's env divides the volume's read by
// (cell_volair.frag airLeakEnv, cellDdgiOpenE). Known answers, four weathers (c3 Foggy noon's DALC, a clear noon,
// a one-sided dusk, a soft overcast; the first three clamp the sky cube, so only the overcast checks amb):
//   amb      rule off: openIrradiance(skyRadianceCube(amb)) = amb on every axis, |rel err| <= 0.5 % (the cube's own
//            defining property; skipped where the cube clamped)
//   up       rule on: +Z equals the rule-off +Z (the upper hemisphere is the same sky), |rel err| <= 1e-4
//   down     rule on: -Z = pi x ground exactly (the whole lower hemisphere is ground), |rel err| <= 0.5 %
//   side     rule on: +X = half the rule-off sky's +X from above plus pi / 2 ground from below, the quadrature of
//            the halves (|rel err| <= 0.5 %)
// Red -DFOGENV_RED_NOPI (the open field without the cosine weight: the solid-angle mean) must fail amb.
// g++ -O2 -std=c++17 -I src tests/features/air/fogleak/fogenv_open_test.cpp -o <out>/fogenv_open.exe; run under timeout 30.
#include "rtddgimath.h"

#include <cmath>
#include <cstdio>

using namespace rtddgi;

static int fails = 0, checks = 0;
static void check( const char * name, bool ok, double v )
{
	checks++;
	if ( !ok )
		fails++;
	std::printf( "%s %s (%.6g)\n", ok ? "PASS" : "FAIL", name, v );
}

#ifdef FOGENV_RED_NOPI
static void openE( const float R[6][3], const float g[3], bool rule, float E[6][3] )
{
	openIrradiance( R, g, rule, E );
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			E[a][c] *= 2.0f / 3.0f;   // red: a wrong weight (what a missing cosine would read on a flat sky)
}
#else
#define openE openIrradiance
#endif

int main()
{
	// DALC linear per axis +X -X +Y -Y +Z -Z (what a surface facing that axis takes)
	const float W[4][6][3] = {
		{ { .21f, .23f, .25f }, { .20f, .22f, .24f }, { .21f, .23f, .25f }, { .20f, .22f, .24f }, { .22f, .24f, .28f }, { .12f, .13f, .14f } },
		{ { .30f, .35f, .45f }, { .28f, .33f, .43f }, { .29f, .34f, .44f }, { .29f, .34f, .44f }, { .40f, .48f, .62f }, { .15f, .14f, .12f } },
		{ { .60f, .30f, .10f }, { .02f, .02f, .03f }, { .10f, .08f, .07f }, { .10f, .08f, .07f }, { .15f, .12f, .14f }, { .02f, .02f, .02f } },
		{ { .21f, .23f, .25f }, { .20f, .22f, .24f }, { .21f, .23f, .25f }, { .20f, .22f, .24f }, { .22f, .24f, .27f }, { .19f, .21f, .22f } } };
	const float alb[3] = { 0.25f, 0.24f, 0.22f };
	for ( int w = 0; w < 4; w++ ) {
		float R[6][3], g[3], Eoff[6][3], Eon[6][3];
		const int clamped = skyRadianceCube( W[w], R );
		groundRadiance( alb, W[w][4], false, g );
		openE( R, g, false, Eoff );
		openE( R, g, true, Eon );
		double eAmb = 0, eUp = 0, eDown = 0, eSide = 0;
		for ( int c = 0; c < 3; c++ ) {
			for ( int a = 0; a < 6; a++ )
				eAmb = std::fmax( eAmb, std::fabs( Eoff[a][c] / W[w][a][c] - 1.0 ) );
			eUp = std::fmax( eUp, std::fabs( Eon[4][c] / Eoff[4][c] - 1.0 ) );
			eDown = std::fmax( eDown, std::fabs( Eon[5][c] / ( kPi * g[c] ) - 1.0 ) );
			// +X: the upper half of the rule-off field is sky either way; the lower half is ground (pi / 2 x g)
			float Rlo[6][3], gz[3] = { 0, 0, 0 }, Eup[6][3];
			for ( int a = 0; a < 6; a++ )
				for ( int k = 0; k < 3; k++ )
					Rlo[a][k] = R[a][k];
			openE( Rlo, gz, true, Eup );   // the sky above, black ground
			eSide = std::fmax( eSide, std::fabs( Eon[0][c] / ( Eup[0][c] + 0.5 * kPi * g[c] ) - 1.0 ) );
		}
		char n[64];
		std::snprintf( n, sizeof n, "w%d amb (cube clamped %d)", w, clamped );
		if ( clamped == 0 )
			check( n, eAmb <= 0.005, eAmb );
		std::snprintf( n, sizeof n, "w%d up", w );
		check( n, eUp <= 1e-4, eUp );
		std::snprintf( n, sizeof n, "w%d down", w );
		check( n, eDown <= 0.005, eDown );
		std::snprintf( n, sizeof n, "w%d side", w );
		check( n, eSide <= 0.005, eSide );
	}
	std::printf( "FOGENV-OPEN %s %d of %d\n", fails ? "FAIL" : "PASS", checks - fails, checks );
	return fails ? 1 : 0;
}
