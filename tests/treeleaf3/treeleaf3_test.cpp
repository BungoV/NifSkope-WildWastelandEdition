// lane TREELEAF3 (FIX1030C): an alpha-tested effect shape's TAA object-vector test keeps exactly the texels its colour
// pass keeps (src/gl/wwfxcov.h). Pure header, no Qt, no GL:
//   g++ -std=c++17 -O2 -I<tree>/src treeleaf3_test.cpp            green: must print "... 0 failed" and exit 0
//   -DTREELEAF3_RED_RAW      today's rule (WwFxCov::side with red = true: base.a x 1, `>=`): must FAIL
//   -DTREELEAF3_RED_NOSTRICT the composed alpha but today's `>=` threshold: must FAIL (the strict threshold is needed)
// The inputs are the census's own (tests/treeleaf3/fx_census.py): base colour alphas of the vanilla movers (bloatfly /
// bloodbug wings 0.75, stingwing 0.8, gulper 0.4), refs 1..254 of both kinds (a material `>`, NIF function 4 `>` and
// 6 `>=`), the vertex alpha on and off, the palette's alpha on (a synthetic palette with steps and ramps) and off, every
// 8-bit base alpha and a set of 8-bit vertex alphas.
#include "gl/wwfxcov.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int checks = 0, failed = 0;

static void check( bool ok, const char * what )
{
	checks++;
	if ( !ok ) {
		failed++;
		if ( failed <= 12 )
			std::printf( "FAIL %s\n", what );
	}
}

// a palette's .a as the GPU reads it with textureLod at (u, v): a 256 x 16 RGBA8 strip, nearest texel (the test only
// needs the SAME function on both sides; it is a step + ramp mix so different (u, v) give different answers)
static float palette( float u, float v )
{
	int x = int( std::floor( std::fmin( std::fmax( u, 0.0f ), 1.0f ) * 255.0f + 0.5f ) );
	int y = int( std::floor( std::fmin( std::fmax( v, 0.0f ), 1.0f ) * 15.0f + 0.5f ) );
	int a = ( ( x * ( y + 1 ) ) / 16 + ( x > 128 ? 40 : 0 ) ) % 256;
	return float( a ) / 255.0f;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const float baseColors[] = { 1.0f, 0.75f, 0.8f, 0.4f, 0.9f, 0.5f };
	const int vtxs[] = { 0, 1, 37, 64, 128, 200, 254, 255 };
	long texelsDiffer = 0, texelsAt = 0;
	char buf[256];
	for ( int kind = 0; kind < 3; kind++ ) {	// 0 a material (`>`), 1 NIF function 4 (`>`), 2 NIF function 6 (`>=`)
		for ( int ref = 1; ref <= 254; ref++ ) {
			for ( float bc : baseColors ) {
				for ( int flags = 0; flags < 4; flags++ ) {	// 1 vertex alpha, 2 the palette's alpha
					WwFxCov::In in;
					in.effect = true;
					in.baseColorA = bc;
					in.vertexAlpha = ( flags & 1 ) != 0;
					in.greyscaleAlpha = ( flags & 2 ) != 0;
					in.strict = kind != 2;
					in.threshold = float( ref ) / 255.0f;	// wwDrawAlpha: ref / 255 (glProperty: Threshold / 255.0)
#if defined( TREELEAF3_RED_RAW )
					const WwFxCov::Side s = WwFxCov::side( in, true );
#else
					WwFxCov::Side s = WwFxCov::side( in, false );
#if defined( TREELEAF3_RED_NOSTRICT )
					s.threshold = in.threshold;
#endif
#endif
					bool same = true;
					for ( int b = 0; b <= 255; b++ ) {
						for ( int v : vtxs ) {
							const float base = float( b ) / 255.0f, vtx = float( v ) / 255.0f;
							const float ac = WwFxCov::colourAlpha( in, base, vtx, palette );
							const float as = WwFxCov::sideAlpha( s, base, vtx, palette );
							const bool kc = WwFxCov::colourKeeps( ac, in.threshold, in.strict );
							const bool ks = WwFxCov::sideKeeps( as, s.threshold );
							if ( kc != ks ) {
								same = false;
								texelsDiffer++;
								texelsAt += ac == in.threshold;
							}
						}
					}
					std::snprintf( buf, sizeof( buf ), "kind %d ref %d baseColor %.2f vertexAlpha %d palette %d: side keeps "
						"the colour pass's texel set", kind, ref, double( bc ), flags & 1, flags >> 1 );
					check( same, buf );
				}
			}
		}
	}
	// the rule's own shape: a lighting shape keeps today's uniforms (mode 0, its alpha, its threshold) bit for bit
	{
		WwFxCov::In in;
		in.lightingAlpha = 0.625f;
		in.threshold = std::nextafter( 0.5f, 2.0f );
		const WwFxCov::Side s = WwFxCov::side( in, false );
		check( s.mode == 0 && s.mult == 0.625f && s.threshold == in.threshold, "a lighting shape keeps today's uniforms" );
		const WwFxCov::Side r = WwFxCov::side( in, true );
		check( r.mode == 0 && r.mult == 0.625f && r.threshold == in.threshold, "the red keeps them too" );
		for ( int b = 0; b <= 255; b++ ) {
			const float base = float( b ) / 255.0f;
			check( WwFxCov::sideAlpha( s, base, 0.3f, palette ) == base * 0.625f, "mode 0 = base.a x alpha (today's GLSL)" );
		}
		WwFxCov::In fx;
		fx.effect = true;
		fx.baseColorA = 0.75f;
		fx.threshold = 0.25f;
		fx.lightingAlpha = 0.625f;	// ignored for an effect
		const WwFxCov::Side e = WwFxCov::side( fx, true );
		check( e.mode == 0 && e.mult == 0.625f && e.threshold == 0.25f, "the red: an effect shape draws today's test" );
		const WwFxCov::Side g = WwFxCov::side( fx, false );
		check( g.mode == 4 && g.mult == 0.5625f && g.threshold == 0.25f, "green: mode 4, baseColor.a^2, `>=` kept for a NIF >=" );
	}
	std::printf( "texels where the side pass and the colour pass disagree: %ld (%ld of them exactly on the threshold)\n",
		texelsDiffer, texelsAt );
	std::printf( "TREELEAF3 rule: %d checks, %d failed\n", checks, failed );
	return failed ? 1 : 0;
}
