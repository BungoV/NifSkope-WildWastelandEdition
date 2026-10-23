/* lane FXPARITY1: unit gate for src/gl/fxparityrules.h (the game's particle modifier rules).
 * Build: g++ -std=c++17 -I<tree>/src tests/fxparity/fxparity_rules_test.cpp -o fxp.exe  (exit 0 = PASS)
 * Red:   add -DFXP_RED: the rules under test become the viewer's old ones, and the gate must FAIL. */
#include "gl/fxparityrules.h"

#include <cmath>
#include <cstdio>

namespace
{
int fails = 0;

void check( bool ok, const char * what, double got, double want )
{
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s: got %.6f want %.6f\n", what, got, want );
	}
}

#ifdef FXP_RED
// the viewer's rules before the lane (src/gl/controllers.cpp particleColor, the drag decay, the frame rate)
void colorUnderTest( const float f[6], const float col[3][4], float u, float o[4] )
{
	const float fi = f[0], fo = f[1], c1End = f[2], c2Start = f[3], c2End = f[4], c3Start = f[5];
	float c[4];
	auto set = [&]( int k ) { for ( int i = 0; i < 4; i++ ) c[i] = col[k][i]; };
	auto mix = [&]( int k, float t ) { for ( int i = 0; i < 4; i++ ) c[i] = col[k][i] + ( col[k + 1][i] - col[k][i] ) * t; };
	if ( u < c1End || c2Start <= c1End )
		set( 0 );
	else if ( u < c2Start )
		mix( 0, ( u - c1End ) / ( c2Start - c1End ) );
	else if ( u < c2End || c3Start <= c2End )
		set( 1 );
	else if ( u < c3Start )
		mix( 1, ( u - c2End ) / ( c3Start - c2End ) );
	else
		set( 2 );
	float a = c[3];
	if ( fi > 0.0f && u < fi )
		a *= u / fi;
	if ( fo < 1.0f && u > fo )
		a *= ( 1.0f - u ) / ( 1.0f - fo );
	o[0] = c[0]; o[1] = c[1]; o[2] = c[2]; o[3] = a > 0.0f ? a : 0.0f;
}
float dragUnderTest( float p, float dt ) { return 1.0f - std::exp( -p * 30.0f * dt ); }
#else
void colorUnderTest( const float f[6], const float col[3][4], float u, float o[4] ) { FxRules::simpleColor( f, col, u, o ); }
float dragUnderTest( float p, float dt ) { return FxRules::dragShare( p, dt ); }
#endif

/* The reference: the game's update transcribed branch for branch from Todd's treat, with its
 * own names: fi fo, colour 1 end (a), colour 2 begins (b), colour 2 end (c), colour 3 begins (d). */
void reference( const float f[6], const float col[3][4], float u, float o[4] )
{
	const float fi = f[0], fo = f[1], a = f[2], b = f[3], c = f[4], d = f[5];
	for ( int i = 0; i < 3; i++ ) {
		if ( b - a == 0.0f || b <= u ) {
			if ( d - c == 0.0f || u <= c )
				o[i] = col[1][i];
			else if ( u < d || u == d )
				o[i] = ( col[2][i] - col[1][i] ) * ( ( u - c ) / ( d - c ) ) + col[1][i];
			else
				o[i] = col[2][i];
		} else if ( u < a ) {
			o[i] = col[0][i];
		} else {
			o[i] = ( col[1][i] - col[0][i] ) * ( ( u - a ) / ( b - a ) ) + col[0][i];
		}
	}
	if ( fo == 0.0f || u <= fo )
		o[3] = ( fi == 0.0f || fi <= u ) ? col[1][3] : ( col[1][3] - col[0][3] ) * ( u / fi ) + col[0][3];
	else
		o[3] = ( ( u - fo ) / ( 1.0f - fo ) ) * ( col[2][3] - col[1][3] ) + col[1][3];
}
}	// namespace

int main()
{
	// synthetic sets for every branch: a ramp in, a degenerate first ramp (b == a), a degenerate second (d == c),
	// no fade out (fo 0), no fade in (fi 0)
	const float sets[5][6] = {
		{ 0.05f, 0.5f, 0.0f, 0.1f, 0.2f, 0.7f },
		{ 0.2f, 0.3f, 0.0f, 0.0f, 0.4f, 0.5f },
		{ 0.1f, 0.6f, 0.1f, 0.3f, 0.5f, 0.5f },
		{ 0.1f, 0.0f, 0.2f, 0.4f, 0.6f, 0.8f },
		{ 0.0f, 0.8f, 0.3f, 0.35f, 0.6f, 1.0f },
	};
	const float col[3][4] = { { 1.0f, 0.5f, 0.25f, 0.0f }, { 0.6f, 0.6f, 0.6f, 1.0f }, { 0.1f, 0.2f, 0.3f, 0.0f } };
	double worst = 0.0;
	for ( const auto & f : sets ) {
		for ( int s = 0; s <= 1000; s++ ) {
			const float u = float( s ) / 1000.0f;
			float g[4], r[4];
			colorUnderTest( f, col, u, g );
			reference( f, col, u, r );
			for ( int i = 0; i < 4; i++ )
				worst = std::fmax( worst, std::fabs( double( g[i] ) - double( r[i] ) ) );
		}
	}
	check( worst < 1e-6, "simple colour vs the game's update, worst channel", worst, 0.0 );

	// known answers: halfway through a fade out to a clear colour 3 the alpha is half colour 2's
	{
		float o[4];
		colorUnderTest( sets[0], col, 0.75f, o );
		check( std::fabs( o[3] - 0.5f ) < 1e-6f, "alpha at u 0.75, fade out 0.5", o[3], 0.5 );
		colorUnderTest( sets[1], col, 0.1f, o );	// b == a: colour 2, not colour 1
		check( std::fabs( o[0] - 0.6f ) < 1e-6f, "red at u 0.1 with an empty first ramp", o[0], 0.6 );
		colorUnderTest( sets[3], col, 0.95f, o );	// fo 0: no fade out
		check( std::fabs( o[3] - 1.0f ) < 1e-6f, "alpha at u 0.95 with no fade out", o[3], 1.0 );
	}

	// drag: linear in the step, capped at the whole share
	check( std::fabs( dragUnderTest( 0.0125f, 1.0f / 60.0f ) - 0.00625f ) < 1e-7f, "drag share 0.0125 at 60 Hz",
		dragUnderTest( 0.0125f, 1.0f / 60.0f ), 0.00625 );
	check( dragUnderTest( 0.5f, 0.25f ) == 1.0f, "drag share capped at 1", dragUnderTest( 0.5f, 0.25f ), 1.0 );

	// flipbook: Frame Count 4 over the life = 4 advances, then the loop start (no randomness: fudges 0)
	{
		FxRules::SubTex st;
		st.start = 2; st.end = 4; st.loopStart = 1; st.count = 4;
		int k = -1, steps = 0, frame = 0, changes = 0, last = -1;
		int seq[8] = {}, at[8] = {}, n = 0;
		for ( int s = 0; s < 1000; s++ ) {
			const float u = float( s ) / 1000.0f;
#ifdef FXP_RED
			frame = int( st.start + st.count * u * 2.0f );	// the old reading: Frame Count frames a second, a 2 s life
			if ( frame > st.end )
				frame = int( st.loopStart );
			( void ) k; ( void ) steps;
#else
			FxRules::subTexStep( st, k, steps, frame, u, []( float ) { return 0.0f; } );
#endif
			if ( frame != last ) {
				if ( n < 8 ) {
					at[n] = s;
					seq[n++] = frame;
				}
				changes++;
				last = frame;
			}
		}
		// 4 steps over the life: a new frame at u 0, 0.25, 0.5, 0.75 (the last wraps to the loop start)
		const int want[4] = { 2, 3, 4, 1 }, wantAt[4] = { 0, 250, 500, 750 };
		bool ok = n == 4;
		for ( int i = 0; ok && i < 4; i++ )
			ok = seq[i] == want[i] && at[i] == wantAt[i];
		check( ok, "flipbook frames 2 3 4 1 at u 0 .25 .5 .75 (count of changes)", changes, 4 );
	}

	std::printf( fails ? "FXPARITY RULES: %d FAILED\n" : "FXPARITY RULES: PASS (%d failed)\n", fails );
	return fails ? 1 : 0;
}
