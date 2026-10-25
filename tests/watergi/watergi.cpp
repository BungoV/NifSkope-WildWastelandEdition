// lane PRTP6: WwWaterGiGuard (src/gl/cellwatergi.h) -- a water draw zeroes the GI switches, the next other draw
// on that program gets the pass's values back, and a pass that sets fresh values is never overwritten by old ones.
// g++ -std=c++17 -I<tree>/src tests/watergi/watergi.cpp; red: -DWW_WATERGI_STALE_RED must fail.
#include "gl/cellwatergi.h"
#include <cstdio>

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s  %s\n", ok ? "PASS" : "FAIL", what );
	fails += ok ? 0 : 1;
}

// A stand-in program: the four live uniform values the draw code reads and writes
struct Prog
{
	int v[WwWaterGiGuard::kN];
};

static void drawOne( WwWaterGiGuard & g, Prog & p, bool water )
{
	switch ( g.draw( &p, water ) ) {
	case WwWaterGiGuard::Zero:
		g.save( &p, p.v );
		for ( int & x : p.v )
			x = 0;
		break;
	case WwWaterGiGuard::Restore: {
		const int * s = g.restore( &p );
		for ( int k = 0; k < WwWaterGiGuard::kN; k++ )
			p.v[k] = s[k];
		break;
	}
	case WwWaterGiGuard::None:
		break;
	}
}

static void passSet( WwWaterGiGuard & g, Prog & p, int a, int b, int c, int d )
{
	p.v[0] = a, p.v[1] = b, p.v[2] = c, p.v[3] = d;
	g.passSet( &p );
}

static bool is( const Prog & p, int a, int b, int c, int d )
{
	return p.v[0] == a && p.v[1] == b && p.v[2] == c && p.v[3] == d;
}

int main()
{
	WwWaterGiGuard g;
	Prog p{}, q{};
	passSet( g, p, 1, 1, 1, 1 );
	drawOne( g, p, false );
	check( is( p, 1, 1, 1, 1 ), "a land draw leaves the GI as the pass set it" );
	drawOne( g, p, true );
	check( is( p, 0, 0, 0, 0 ), "a water draw reads no GI" );
	drawOne( g, p, true );
	check( is( p, 0, 0, 0, 0 ), "two water draws in a row stay zeroed" );
	drawOne( g, p, false );
	check( is( p, 1, 1, 1, 1 ), "the next land draw gets the pass's values back" );

	passSet( g, q, 1, 0, 1, 0 );
	drawOne( g, q, true );
	check( is( q, 0, 0, 0, 0 ) && is( p, 1, 1, 1, 1 ), "programs are kept apart" );

	// a new pass after a water draw (the GI row turned off meanwhile): its values win, not the saved ones
	drawOne( g, p, true );
	passSet( g, p, 0, 1, 0, 0 );
	drawOne( g, p, false );
	check( is( p, 0, 1, 0, 0 ), "a fresh pass is never overwritten by the values saved before it" );
	drawOne( g, q, false );
	check( is( q, 1, 0, 1, 0 ), "the other program gets its own values back" );

	std::printf( "%d failures\n%s\n", fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
