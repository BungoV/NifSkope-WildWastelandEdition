/* Lane GLASSPANE2 (FIX1030B round 2, 2026-10-30), FXNORMALA1: the effect shader's env cube x the normal map's alpha.
 *
 * Judges src/gl/glasspanerules.h (wwFxEnvNormalAlphaRule / wwFxEnvNormalAlphaFactor) on a fixture: a 4x4 RGBA8
 * effect normal map whose alpha runs 255 .. 0 (a mod normal; vanilla's are all 255), an env mask's R, a cube colour and
 * an env scale. The game's env effect pixel shader forms, per texel, cube x env scale x normal.a x maskR (the asm
 * order: the cube times the scale, times the normal map's w, then the mask's R in the final mad); the rule must give
 * that term on every texel. Also: the red key (WW_FX_NORMALA_RED) turns the factor off, "0" / "" keep it, and the
 * shader + renderer carry the wiring (res/shaders/fo4_effectshader.frag, src/gl/renderer.cpp).
 *
 * Build: g++ -std=c++17 -I src tests/glasspane2/glasspane2_test.cpp   (run from the tree root; arg 1 = the tree root)
 * Green prints "GLASSPANE2 PASS 4". The red build (-DGLASSPANE2_RED = the base: no factor) must print
 * "FAIL fixture:" and exit 1.
 */
#include "gl/glasspanerules.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static int fails = 0;
static int passes = 0;

static void check( bool ok, const char * what, const std::string & detail )
{
	if ( ok ) {
		++passes;
		std::printf( "ok %s: %s\n", what, detail.c_str() );
	} else {
		++fails;
		std::printf( "FAIL %s: %s\n", what, detail.c_str() );
	}
}

static std::string slurp( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

int main( int argc, char ** argv )
{
	const std::string root = argc > 1 ? argv[1] : ".";

	// 1. the rule's keys
	{
		const bool a = wwFxEnvNormalAlphaRule( nullptr ), b = wwFxEnvNormalAlphaRule( "" ),
			c = wwFxEnvNormalAlphaRule( "0" ), d = wwFxEnvNormalAlphaRule( "1" ), e = wwFxEnvNormalAlphaRule( "yes" );
		char buf[160];
		std::snprintf( buf, sizeof buf, "unset %d, \"\" %d, \"0\" %d (want 1 1 1); red \"1\" %d, \"yes\" %d (want 0 0)",
			a, b, c, d, e );
		check( a && b && c && !d && !e, "keys", buf );
	}

	// 2. the fixture: 16 texels, normal alpha 255 - 17 i (255 .. 0), mask R 200, cube (0.30, 0.45, 0.60), scale 1.5
	{
		const float maskR = 200.0f / 255.0f, scale = 1.5f;
		const float cube[3] = { 0.30f, 0.45f, 0.60f };
		int bad = 0, below1 = 0;
		double worst = 0.0, sumGame = 0.0, sumOurs = 0.0;
		const bool rule = wwFxEnvNormalAlphaRule( nullptr );
		for ( int i = 0; i < 16; ++i ) {
			const unsigned char a8 = (unsigned char)( 255 - 17 * i );
			const float na = a8 / 255.0f;
			below1 += a8 < 255;
			for ( int c = 0; c < 3; ++c ) {
				// the game's order: (cube x scale) x normal.a, then x maskR
				const float game = ( cube[c] * scale ) * na * maskR;
				// ours (fo4_effectshader.frag): cube x (envReflection x s), then x the rule's factor
				const float ours = cube[c] * ( scale * maskR ) * wwFxEnvNormalAlphaFactor( rule, na );
				const double d = std::fabs( double( game ) - double( ours ) );
				worst = d > worst ? d : worst;
				bad += d > 1e-5;
				sumGame += game;
				sumOurs += ours;
			}
		}
		char buf[200];
		std::snprintf( buf, sizeof buf, "%d texels with alpha < 1; cube term off the game's on %d of 48 channels "
			"(worst %.5f), mean ours / game = %.3f", below1, bad, worst, sumOurs / sumGame );
		check( bad == 0 && below1 == 15, "fixture", buf );
	}

	// 3. vanilla does not move: alpha 1 gives factor 1 under the rule and under the red
	{
		const float on = wwFxEnvNormalAlphaFactor( true, 1.0f ), off = wwFxEnvNormalAlphaFactor( false, 1.0f );
		char buf[120];
		std::snprintf( buf, sizeof buf, "alpha 1.000: factor %.3f (rule), %.3f (red)", on, off );
		check( on == 1.0f && off == 1.0f, "vanilla", buf );
	}

	// 4. wiring: the shader multiplies the cube by normalMap.a unless the red uniform is set, the renderer sets it
	{
		const std::string frag = slurp( root + "/res/shaders/fo4_effectshader.frag" );
		const std::string rend = slurp( root + "/src/gl/renderer.cpp" );
		const bool u = frag.find( "uniform bool envNormalAlphaRed;" ) != std::string::npos;
		const size_t at = frag.find( "if ( !envNormalAlphaRed )" );
		const bool m = at != std::string::npos && frag.find( "cube.rgb *= normalMap.a;", at ) != std::string::npos
			&& frag.find( "cube.rgb *= normalMap.a;", at ) < at + 80;
		const bool r = rend.find( "\"envNormalAlphaRed\", !wwFxEnvNormalAlpha()" ) != std::string::npos;
		char buf[160];
		std::snprintf( buf, sizeof buf, "frag uniform %d, frag factor %d, renderer sets it %d (files read: %zu / %zu B)",
			u, m, r, frag.size(), rend.size() );
		check( u && m && r, "wiring", buf );
	}

	if ( fails ) {
		std::printf( "GLASSPANE2 FAILED %d of %d\n", fails, fails + passes );
		return 1;
	}
	std::printf( "GLASSPANE2 PASS %d\n", passes );
	return 0;
}
