// lane LANDSPEC1: src/landlodspec.h against hand-computed values, and the shader's literals against the header.
// g++ -std=c++17 -I<tree>/src tests/landspec/landspec.cpp; run with <tree> as argv[1]. Red: -DLANDSPEC_RED.
#include "landlodspec.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#ifdef LANDSPEC_RED	// the viewer before the lane: the ground keeps its own gloss/spec at every distance
namespace LandLodSpec
{
inline GlossSpec landRed( GlossSpec own, Mode, float, float, float = kScale ) { return own; }
}
#define LAND LandLodSpec::landRed
#else
#define LAND LandLodSpec::land
#endif

static int fails = 0;
static void near( const char * what, float got, float want, float tol = 1e-5f )
{
	const bool ok = std::fabs( got - want ) <= tol;
	std::printf( "%s %s: %.7g want %.7g\n", ok ? "ok" : "FAIL", what, got, want );
	fails += ok ? 0 : 1;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	using namespace LandLodSpec;
	// hand values: blend 0 up to 6963.2, 1 from 9625.6; at 8294.4 (mid) 0.5; at 9000 (9625.6-9000)/2662.4 = 0.234976 -> 0.765024
	near( "blend 0", blend( 0.0f ), 0.0f );
	near( "blend 6963.2", blend( 6963.2f ), 0.0f, 1e-4f );
	near( "blend 8294.4", blend( 8294.4f ), 0.5f, 1e-4f );
	near( "blend 9000", blend( 9000.0f ), 0.765024f, 1e-4f );
	near( "blend 20000", blend( 20000.0f ), 1.0f );
	near( "cell centre 100", cellCentre( 100.0f ), 0.0f );
	near( "cell centre 2049", cellCentre( 2049.0f ), 4096.0f );
	near( "cell centre -2049", cellCentre( -2049.0f ), -4096.0f );
	// a sand texel: _s gloss 0.129, mask 0.5 (material scalars 1)
	const GlossSpec sand{ 0.129f, 0.5f };
	GlossSpec g = LAND( sand, Near, 3000.0f, 0.0f );	// inside 1.7 cells: untouched
	near( "near 3000 gloss", g.gloss, 0.129f );
	near( "near 3000 spec", g.spec, 0.5f );
	g = LAND( sand, Near, 6000.0f, 6000.0f );	// d 8485.28: v = 1 - (9625.6-8485.28)/2662.4 = 0.571697
	near( "near 8485 gloss", g.gloss, 0.129f + ( 0.22f - 0.129f ) * 0.571697f, 1e-4f );
	near( "near 8485 spec", g.spec, 0.5f + ( 0.06f - 0.5f ) * 0.571697f, 1e-4f );
	g = LAND( sand, Near, 12000.0f, 0.0f );
	near( "near 12000 gloss", g.gloss, 0.22f );
	near( "near 12000 spec", g.spec, 0.06f );
	g = LAND( sand, Far, 0.0f, 0.0f );
	near( "far gloss", g.gloss, 0.22f );
	near( "far spec", g.spec, 0.06f );
	g = LAND( sand, Off, 50000.0f, 0.0f );
	near( "off gloss", g.gloss, 0.129f );
	// wet: gloss 0.22 spec 0.06 at wet 0.5: spec' = 0.06*0.85+0.15 = 0.201; gloss 1.875*0.22 = 0.4125; spec 0.75*0.201 = 0.15075
	GlossSpec w = wetLand( GlossSpec{ 0.22f, 0.06f }, 0.5f );
	near( "wet 0.5 gloss", w.gloss, 0.4125f );
	near( "wet 0.5 spec", w.spec, 0.15075f );
	w = wetLand( GlossSpec{ 0.22f, 0.06f }, 0.0f );
	near( "wet 0 gloss", w.gloss, 0.22f );
	near( "wet 0 spec", w.spec, 0.06f );
	w = wetLand( GlossSpec{ 0.3f, 0.2f }, 1.0f );	// spec' 0.2*0.7+0.3 = 0.44 -> x0.5 = 0.22; gloss x2.75 = 0.825
	near( "wet 1 gloss", w.gloss, 0.825f );
	near( "wet 1 spec", w.spec, 0.22f );
	near( "wet albedo 1", wetAlbedoK( 1.0f ), 0.65f );
	near( "wet clock rain 10 s", wetStep( 0.0f, true, 10.0f ), 0.3f );
	near( "wet clock dry 10 s", wetStep( 1.0f, false, 10.0f ), 0.8f );
	// the shader runs the same law: its landLodK comes from these constants, its blend formula must match
	if ( argc > 1 ) {
		std::ifstream f( std::string( argv[1] ) + "/res/shaders/fo4_default.frag" );
		std::stringstream ss;
		ss << f.rdbuf();
		const std::string s = ss.str();
		const char * must[] = { "1.0 - clamp( ( landLodK.z - distance( cellWorldPos( -ViewDir ).xy, landLod.yz ) ) * landLodK.w, 0.0, 1.0 )",
			"smoothness = mix( smoothness, landLodK.x, landLodV );", "specMask = mix( specMask, landLodK.y, landLodV );" };
		for ( const char * m : must ) {
			const bool ok = s.find( m ) != std::string::npos;
			std::printf( "%s shader has: %s\n", ok ? "ok" : "FAIL", m );
			fails += ok ? 0 : 1;
		}
	}
	std::printf( "LANDSPEC %s %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
