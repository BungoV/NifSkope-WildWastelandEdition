// lane LIGHTPHYS2: wwlp::lightTerm (the GPU relight's per-light term, src/proberelight.cpp kGlslDirect) evaluated
// the way the kernel does must equal wwlp::falloff() over many authored curves, radii, modes and reds; keepsGame()
// must say exactly when falloff() is the game's curve; sameBounce() must see every field the bounce reads.
// Red (-DLIGHTTERM_RED): the kernel's advanced branch with its window left on (the nowindow red ignored) must fail.
#include "gl/lightphys.h"

#include <cmath>
#include <cstdio>

static float kernel( const wwlp::LightTerm & t, float d, float r, float bias, float scale, float exponent )
{
	const float x = std::min( std::max( d / std::max( r, 0.001f ), 0.0f ), 1.0f );
	if ( t.kind == 0 ) {
		const float xe = exponent > 0.0f ? std::pow( x, exponent ) : 1.0f;
		const float kk = 1.0f - std::min( std::max( scale * xe + bias, 0.0f ), 1.0f );
		return kk > 0.0f ? std::pow( kk, 2.2f ) : 0.0f;
	}
	if ( t.kind == 1 ) {
		const float s = t.s, x2 = x * x;
		const float w = std::min( std::max( 1.0f - x2 * x2, 0.0f ), 1.0f ), den = x2 + s * s;
		return den > 0.0f ? t.k * ( w * w / den ) : 0.0f;
	}
	const float u = std::min( std::max( ( x - 0.8f ) / ( 1.0f - 0.8f ), 0.0f ), 1.0f );
#ifdef LIGHTTERM_RED
	const float w = 1.0f - u * u * ( 3.0f - 2.0f * u );
#else
	const float w = t.kind == 3 ? 1.0f : 1.0f - u * u * ( 3.0f - 2.0f * u );
#endif
	const float m = std::max( t.x0, x );
	return t.k * w / ( m * m );
}

int main()
{
	int fails = 0, n = 0, game = 0;
	double worst = 0;
	const float biases[] = { 0.0f, 0.05f, 0.2f, -0.1f }, scales[] = { 1.0f, 0.8f, 1.3f, 2.0f, 0.0f };
	const float exps[] = { 2.0f, 1.0f, 0.5f, 3.0f, 0.0f }, radii[] = { 64.0f, 300.0f, 900.0f, 4000.0f };
	for ( int mode = 0; mode < 3; mode++ )
		for ( int red = 0; red < 16; red++ )
			for ( float b : biases )
				for ( float sc : scales )
					for ( float e : exps )
						for ( float r : radii ) {
							wwlp::Params p;
							p.mode = mode;
							p.red = red;
							p.advNumerator = wwlp::vanillaFlux() * 1.5f;
							const wwlp::LightTerm t = wwlp::lightTerm( p, r, b, sc, e );
							const bool keeps = wwlp::keepsGame( p, b, sc, e );
							bool allGame = true;
							for ( int i = 0; i <= 64; i++ ) {
								const float d = r * float( i ) / 64.0f;
								const float want = wwlp::falloff( p, d, r, b, sc, e );
								const float got = kernel( t, d, r, b, sc, e );
								const float van = wwlp::attVanilla( d / r, b, sc, e );
								allGame = allGame && want == van;
								const double rel = std::fabs( double( got ) - want ) / std::max( 1e-6, std::fabs( double( want ) ) );
								worst = std::max( worst, rel );
								n++;
								if ( rel > 1e-5 ) {
									if ( fails < 5 )
										std::printf( "FAIL mode %d red %d b %g s %g e %g r %g d %g: kernel %.9g falloff %.9g\n", mode, red,
											b, sc, e, r, d, got, want );
									fails++;
								}
							}
							if ( keeps != ( t.kind == 0 ) || ( keeps && !allGame ) ) {
								if ( fails < 5 )
									std::printf( "FAIL keepsGame %d kind %d allGame %d (mode %d red %d b %g s %g e %g)\n", keeps, t.kind,
										allGame, mode, red, b, sc, e );
								fails++;
							}
							game += keeps;
						}
	// sameBounce: each field the bounce reads makes it false, the area size (specular only) does not
	wwlp::Params a, b;
	a.mode = b.mode = wwlp::Advanced;
	b.advNumerator = 1.0f;
	fails += wwlp::sameBounce( a, b ) ? 1 : 0;
	b = a;
	b.areaWorld = 8.0f;
	fails += wwlp::sameBounce( a, b ) ? 0 : 1;
	b = a;
	b.curve = 1.5f;
	fails += wwlp::sameBounce( a, b ) ? 1 : 0;
	a.mode = b.mode = wwlp::Physical;
	a.curve = b.curve = 1.0f;
	b.sourceFraction = 0.2f;
	fails += wwlp::sameBounce( a, b ) ? 1 : 0;
	b = a;
	b.red = 1;
	fails += wwlp::sameBounce( a, b ) ? 1 : 0;
	b = a;
	b.advNumerator = 3.0f;	// physical reads no numerator
	fails += wwlp::sameBounce( a, b ) ? 0 : 1;
	std::printf( "lightterm: %d samples, %d curves on the game's curve, worst rel %.3g, %d failures\n", n, game, worst, fails );
	return fails ? 1 : 0;
}
