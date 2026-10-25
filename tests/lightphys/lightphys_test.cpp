// lane LIGHTPHYS1: src/gl/lightphys.h against values computed from the reference implementation (fixtures.inc,
// written by notes/lightphys1/parity). Off must be the game's float. -DLIGHTPHYS_RED tests the game's curve
// everywhere (what the base drew) and must fail.
// Build: g++ -std=c++17 -O2 -Wall -I<tree>/src tests/lightphys/lightphys_test.cpp
#include "gl/lightphys.h"

#include <cmath>
#include <cstdio>

struct Fix { int kind; float a, b, c, x, r, v; };
static const Fix kFix[] = {
#include "fixtures.inc"
};

int main()
{
	int bad = 0, n = 0;
	auto check = [&]( const char * what, double got, double want, double tol ) {
		n++;
		const double e = std::fabs( got - want ) / std::max( std::fabs( want ), 1.0e-6 );
		if ( e > tol ) {
			if ( bad < 10 )
				std::printf( "FAIL %s got %.9g want %.9g\n", what, got, want );
			bad++;
		}
	};
	for ( const Fix & f : kFix ) {
		wwlp::Params p;
		p.advNumerator = wwlp::vanillaFlux() * wwlp::kAdvCalibration;
		switch ( f.kind ) {
		case 1:
		case 2:
			p.mode = f.kind == 1 ? wwlp::Physical : wwlp::Advanced;
#ifdef LIGHTPHYS_RED
			p.mode = wwlp::Vanilla;
#endif
			check( f.kind == 1 ? "physical" : "advanced", wwlp::falloff( p, f.x * f.r, f.r, f.a, f.b, f.c ), f.v, 1.0e-6 );
			break;
		case 3: {
			p.curve = f.a;
			const float col[3] = { f.x, f.x, f.x };
#ifdef LIGHTPHYS_RED
			p.curve = 1.0f;
#endif
			check( "intensity", wwlp::intensityScale( p, col ), f.v, 1.0e-6 );
			break;
		}
		case 4:
			check( "area", wwlp::areaWidenedRough( f.a, f.b, wwlp::areaSize( f.r, wwlp::kSourceFraction, wwlp::kSourceWorld ) ), f.v,
				1.0e-6 );
			break;
		}
	}
	// Off is the game's curve, the same float
	wwlp::Params off;
	for ( int i = 0; i <= 100; i++ ) {
		const float x = float( i ) / 100.0f;
		n++;
		if ( wwlp::falloff( off, x * 300.0f, 300.0f, 0.0f, 1.0f, 2.0f ) != wwlp::attVanilla( x * 300.0f / 300.0f, 0.0f, 1.0f, 2.0f ) )
			bad++;
	}
	const float white[3] = { 1.0f, 1.0f, 1.0f };
	n++;
	if ( wwlp::intensityScale( off, white ) != 1.0f )
		bad++;
	std::printf( "%d checks, %d failed\n%s\n", n, bad, bad ? "FAIL" : "PASS" );
	return bad ? 1 : 0;
}
