/* lane PERFFIX2 b (item 8b): gl/sunshadowfit.h cascadesForTier + sliceFars, the cascade count per Cascade Quality tier.
 * Green: Low draws 2, Medium / High / Ultra / Off 3, the red switch gives 3 everywhere; with 2 the receivers' pair
 * selection (a twin of ww_sunshadow.glsl csmSelect) never reaches cascade 2 out to the fade, and every view depth up to
 * the reach lies in the slice of each cascade that gets weight; with 3 the slices are today's (split0, split1, D).
 * Reds (each must FAIL): -DCASCADES_RED_TODAY (3 at Low: the count is today's) and -DCASCADES_RED_RECV (the receivers
 * still hand the far half to the undrawn cascade 2). The presets are shadowset.cpp's (checked in the source text). */
#include "features/shadows/sunshadowfit.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static int fails = 0;

static void check( bool ok, const char * what, const std::string & detail = std::string() )
{
	std::printf( "%s %s%s%s\n", ok ? "ok" : "FAIL", what, detail.empty() ? "" : ": ", detail.c_str() );
	if ( !ok )
		fails++;
}

// ww_sunshadow.glsl csmSelect, and the smoothstep weight
static void select( double dv, double split0, double recvSplit1, double band, int & a, int & b, double & t )
{
	double bound;
	if ( dv < recvSplit1 ) {
		a = 0;
		b = 1;
		bound = split0;
	} else {
		a = 1;
		b = 2;
		bound = recvSplit1;
	}
	const double x = std::fmin( std::fmax( ( dv - bound ) / band, 0.0 ), 1.0 );
	t = x * x * ( 3.0 - 2.0 * x );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	// the tiers as shadowset.cpp's wwShadowCascadePreset sets them (D, split0, split1, blend)
	struct T { int tier; double D, s0, s1, B; } tiers[] = { { 1, 4000, 900, 2600, 150 }, { 2, 6000, 1100, 3400, 200 },
		{ 3, 10000, 1500, 4800, 300 }, { 4, 16000, 2000, 6500, 400 } };
	check( WwCsmFit::cascadesForTier( 1, false ) == 2, "Low draws 2 cascades" );
	check( WwCsmFit::cascadesForTier( 0, false ) == 3 && WwCsmFit::cascadesForTier( 2, false ) == 3
		&& WwCsmFit::cascadesForTier( 3, false ) == 3 && WwCsmFit::cascadesForTier( 4, false ) == 3, "Off, Medium, High, Ultra draw 3" );
	check( WwCsmFit::cascadesForTier( 1, true ) == 3 && WwCsmFit::cascadesForTier( 4, true ) == 3, "the red: 3 at every tier" );

	for ( const T & t : tiers ) {
		const int n = WwCsmFit::cascadesForTier( t.tier, false );
		double far[3], recv = 0.0;
		WwCsmFit::sliceFars( n, t.s0, t.s1, t.D, t.B, far, recv );
		const double zn[3] = { 0.0, far[0] - t.B, far[1] - t.B }, zf[3] = { far[0] + t.B, far[1] + t.B, far[2] + t.B };
		bool noC2 = true, covered = true;
		char where[160] = "";
		for ( double dv = 0.0; dv <= t.D + t.B; dv += 0.5 ) {
			int a, b;
			double w;
			select( dv, t.s0, recv, t.B, a, b, w );
			if ( n == 2 && ( a >= 2 || ( b >= 2 && w > 0.0 ) ) ) {
				if ( noC2 )
					std::snprintf( where, sizeof where, "tier %d: cascade %d at view depth %.1f", t.tier, b, dv );
				noC2 = false;
			}
			if ( dv <= t.D ) {
				const bool okA = w >= 1.0 || ( dv >= zn[a] && dv <= zf[a] );
				const bool okB = w <= 0.0 || ( dv >= zn[b] && dv <= zf[b] );
				if ( !okA || !okB ) {
					if ( covered && !where[0] )
						std::snprintf( where, sizeof where, "tier %d: depth %.1f outside cascade %d's slice", t.tier, dv, okA ? b : a );
					covered = false;
				}
			}
		}
		char what[120];
		std::snprintf( what, sizeof what, "tier %d (%d cascades): no undrawn cascade selected, every depth in its slices", t.tier, n );
		check( noC2 && covered, what, where );
		if ( n == 3 )
			check( far[0] == t.s0 && far[1] == t.s1 && far[2] == t.D && recv == t.s1, "  3 cascades: today's slices" );
	}

	// the presets in the test are shadowset.cpp's, and sunshadow.cpp draws only the count
	if ( argc > 1 ) {
		auto read = [&]( const char * rel ) {
			std::ifstream in( std::string( argv[1] ) + rel );
			std::stringstream ss;
			ss << in.rdbuf();
			return ss.str();
		};
		const std::string set = read( "/src/features/shadows/shadowset.cpp" ), sun = read( "/src/features/shadows/sunshadow.cpp" );
		check( set.find( "p.D = 4000; p.split0 = 900; p.split1 = 2600; p.blend = 150;" ) != std::string::npos
			&& set.find( "p.D = 16000; p.split0 = 2000; p.split1 = 6500; p.blend = 400;" ) != std::string::npos,
			"the tiers' numbers are shadowset.cpp's" );
		check( set.find( "WwCsmFit::cascadesForTier" ) != std::string::npos, "shadowset.cpp takes the count from cascadesForTier" );
		check( sun.find( "WwCsmFit::sliceFars" ) != std::string::npos && sun.find( "i >= s.nCasc" ) != std::string::npos,
			"sunshadow.cpp fits with sliceFars and draws only s.nCasc cascades" );
	}
	std::printf( "CASCADECOUNT %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
