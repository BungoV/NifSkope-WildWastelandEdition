// Lane SKYBELOW2 (2026-10-31): SKYBELOW1's below-horizon rule for the reflection probes' relight, unit-tested on the
// shader's OWN text: res/shaders/cell_skybelow.glsl is compiled here through tests/rtddgi_math/glsl_shim.h and checked
// against SKYBELOW1's header rule (src/rtddgimath.h belowHorizon / groundRadiance). One check per rule, one red each
// (-DSKYBELOW2_RED_<NAME> changes the shader file's text as compiled here; each must FAIL its own check):
//   below      a miss below the horizon returns the ground, exterior = albedo x skyUp ==       RED_MISS      the sky
//              rtddgimath.h groundRadiance( albedo, pi skyUp ) (the same horizon d.z < 0)                     below too
//   above      a miss on or above the horizon is the sky ambient, bit for bit, rule on == off  RED_LIFT      horizon 0.1
//   interior   an interior's below-horizon miss is 0, its above-horizon miss the ambient       RED_INTGROUND ground lit
//   red        rule 0 (WW_SKYBELOW_RED) returns today's miss everywhere, bit for bit           RED_NOSWITCH  rule forced
//   albedo     the 9 ground taps are unit, below the horizon, distinct; the albedo is the     RED_ALBEDO    a missed tap
//              mean of the taps that hit, 0.5 when none hit                                                 is black
//   openprobe  the census: a probe 100 u over a ground disk (r 3000 u), its 64 x 64              RED_MISS
//              octahedral tile as cellreflprobe relights it: below-horizon misses returning
//              sky 0 (the red: all of them), above-horizon misses unchanged (hash equal)
// Every check is independent of the others' reds (a red build may pass the other five).

#include "rtddgimath.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace glsl {
#include "../rtddgi_math/glsl_shim.h"
#include "../../res/shaders/cell_skybelow.glsl"
}

static int g_fail = 0;
static void report( const char * check, bool ok, const char * fmt, double a, double b )
{
	char msg[256];
	std::snprintf( msg, sizeof msg, fmt, a, b );
	std::printf( "%s %s: %s\n", ok ? "ok" : "FAIL", check, msg );
	if ( !ok )
		g_fail++;
}

static glsl::vec3 fib( int i, int n )
{
	const double z = 1.0 - ( i + 0.5 ) * 2.0 / n, r = std::sqrt( std::max( 0.0, 1.0 - z * z ) );
	const double ga = 3.14159265358979323846 * ( 3.0 - std::sqrt( 5.0 ) );
	return glsl::vec3( std::cos( ga * i ) * r, std::sin( ga * i ) * r, z );
}

// a sky ambient per direction, like rpAmbient (a six-colour cube, n^2 weights): blue above, grey sides, dark below
static glsl::vec3 skyAt( glsl::vec3 d )
{
	const double up[3] = { 0.30, 0.42, 0.65 }, side[3] = { 0.22, 0.27, 0.36 }, dn[3] = { 0.08, 0.075, 0.07 };
	const double wz = d.z * d.z, ws = 1.0 - wz;
	const double * v = d.z >= 0.0f ? up : dn;
	return glsl::vec3( ws * side[0] + wz * v[0], ws * side[1] + wz * v[1], ws * side[2] + wz * v[2] );
}

static bool same( glsl::vec3 a, glsl::vec3 b ) { return a.x == b.x && a.y == b.y && a.z == b.z; }

static std::uint64_t mixh( std::uint64_t h, float v )
{
	std::uint32_t u;
	std::memcpy( &u, &v, 4 );
	h ^= u;
	h *= 1099511628211ull;
	return h;
}

// cell_reflrelight.frag rpOctDecode
static glsl::vec3 octDecode( double u, double v )
{
	double fx = u * 2.0 - 1.0, fy = v * 2.0 - 1.0, fz = 1.0 - std::fabs( fx ) - std::fabs( fy );
	const double t = std::max( -fz, 0.0 );
	fx += fx >= 0.0 ? -t : t;
	fy += fy >= 0.0 ? -t : t;
	const double l = std::sqrt( fx * fx + fy * fy + fz * fz );
	return glsl::vec3( fx / l, fy / l, fz / l );
}

int main()
{
	const int N = 4096;
	const glsl::vec3 alb( 0.21, 0.19, 0.16 ), skyUp = skyAt( glsl::vec3( 0.0, 0.0, 1.0 ) );

	{	// below
		int belowN = 0, retSky = 0, lawBad = 0, horizonBad = 0;
		double worst = 0.0;
		for ( int i = 0; i < N; i++ ) {
			const glsl::vec3 d = fib( i, N );
			if ( !( d.z < 0.0f ) )
				continue;
			belowN++;
			const glsl::vec3 sky = skyAt( d );
			const glsl::vec3 m = glsl::wwSkyBelowMiss( d, sky, alb, skyUp, false, 1 );
			if ( same( m, sky ) )
				retSky++;
			if ( !rtddgi::belowHorizon( rtddgi::V3( d.x, d.y, d.z ) ) )
				horizonBad++;
			const float a3[3] = { alb.x, alb.y, alb.z };
			const float e3[3] = { float( rtddgi::kPi * skyUp.x ), float( rtddgi::kPi * skyUp.y ), float( rtddgi::kPi * skyUp.z ) };
			float g[3];
			rtddgi::groundRadiance( a3, e3, false, g );
			const double err = std::max( { std::fabs( g[0] - m.x ), std::fabs( g[1] - m.y ), std::fabs( g[2] - m.z ) } );
			worst = std::max( worst, err );
			if ( err > 1e-6 )
				lawBad++;
		}
		report( "below", retSky == 0 && lawBad == 0 && horizonBad == 0 && belowN > 0,
			"below-horizon misses returning sky %.0f of %.0f (ground law == rtddgimath.h groundRadiance)", retSky, belowN );
		std::printf( "   below: ground law max |diff| %.3g, horizon disagreements %d\n", worst, horizonBad );
	}
	{	// above
		int aboveN = 0, changed = 0;
		for ( int i = 0; i < N; i++ ) {
			const glsl::vec3 d = fib( i, N );
			if ( d.z < 0.0f )
				continue;
			aboveN++;
			const glsl::vec3 sky = skyAt( d );
			if ( !same( glsl::wwSkyBelowMiss( d, sky, alb, skyUp, false, 1 ), sky )
				|| !same( glsl::wwSkyBelowMiss( d, sky, alb, skyUp, false, 1 ), glsl::wwSkyBelowMiss( d, sky, alb, skyUp, false, 0 ) ) )
				changed++;
		}
		const glsl::vec3 h0( 1.0, 0.0, 0.0 );	// exactly on the horizon: sky
		if ( !same( glsl::wwSkyBelowMiss( h0, skyAt( h0 ), alb, skyUp, false, 1 ), skyAt( h0 ) ) )
			changed++;
		report( "above", changed == 0, "on/above-horizon misses changed %.0f of %.0f (must be 0: the sky, bit for bit)", changed, aboveN + 1 );
	}
	{	// interior
		int bad = 0, n = 0;
		for ( int i = 0; i < N; i++ ) {
			const glsl::vec3 d = fib( i, N );
			const glsl::vec3 sky = skyAt( d );
			const glsl::vec3 m = glsl::wwSkyBelowMiss( d, sky, alb, skyUp, true, 1 );
			n++;
			if ( d.z < 0.0f ? !same( m, glsl::vec3( 0.0 ) ) : !same( m, sky ) )
				bad++;
		}
		report( "interior", bad == 0, "interior misses off the rule %.0f of %.0f (below 0, above the ambient)", bad, n );
	}
	{	// red
		int bad = 0, n = 0;
		for ( int in = 0; in < 2; in++ )
			for ( int i = 0; i < N; i++ ) {
				const glsl::vec3 d = fib( i, N );
				const glsl::vec3 sky = skyAt( d );
				n++;
				if ( !same( glsl::wwSkyBelowMiss( d, sky, alb, skyUp, in != 0, 0 ), sky ) )
					bad++;
			}
		report( "red", bad == 0, "rule 0 (WW_SKYBELOW_RED) misses not today's sky %.0f of %.0f", bad, n );
	}
	{	// albedo
		int bad = 0;
		for ( int k = 0; k < 9; k++ ) {
			const glsl::vec3 t = glsl::wwSkyBelowTap( k );
			if ( std::fabs( glsl::length( t ) - 1.0 ) > 1e-6 || !( t.z < 0.0f ) )
				bad++;
			for ( int j = 0; j < k; j++ )
				if ( glsl::dot( t, glsl::wwSkyBelowTap( j ) ) > 0.999 )
					bad++;
		}
		// 5 of 9 taps hit a 0.2 ground, 4 missed
		const glsl::vec3 sum( 5 * 0.2, 5 * 0.2, 5 * 0.2 );
		const glsl::vec3 a = glsl::wwSkyBelowAlbedo( sum, 5.0f ), none = glsl::wwSkyBelowAlbedo( glsl::vec3( 0.0 ), 0.0f );
		const double e = std::max( { std::fabs( a.x - 0.2 ), std::fabs( a.y - 0.2 ), std::fabs( a.z - 0.2 ) } );
		if ( e > 1e-6 || !same( none, glsl::vec3( 0.5 ) ) )
			bad++;
		report( "albedo", bad == 0, "albedo of 5 hits of 0.2: %.4f (taps/fallback faults %.0f)", a.x, bad );
	}
	{	// openprobe: the census on a 64 x 64 tile, a probe 100 u over a ground disk of radius 3000 u
		const double H = 100.0, R = 3000.0;
		const int F = 64;
		long belowMiss = 0, belowMissSky = 0, aboveMiss = 0, belowHit = 0;
		std::uint64_t hRule = 1469598103934665603ull, hRed = hRule;
		double lowRule = 0.0, lowRed = 0.0;
		for ( int y = 0; y < F; y++ )
			for ( int x = 0; x < F; x++ ) {
				const glsl::vec3 d = octDecode( ( x + 0.5 ) / F, ( y + 0.5 ) / F );
				bool hit = false;
				if ( d.z < 0.0f ) {
					const double t = H / -d.z, px = d.x * t, py = d.y * t;
					hit = px * px + py * py <= R * R;
				}
				if ( hit ) {
					belowHit++;
					continue;
				}
				const glsl::vec3 sky = skyAt( d );
				const glsl::vec3 m = glsl::wwSkyBelowMiss( d, sky, alb, skyUp, false, 1 );
				const glsl::vec3 r = glsl::wwSkyBelowMiss( d, sky, alb, skyUp, false, 0 );
				if ( d.z < 0.0f ) {
					belowMiss++;
					belowMissSky += same( m, sky ) ? 1 : 0;
					lowRule += m.y;
					lowRed += r.y;
				} else {
					aboveMiss++;
					hRule = mixh( mixh( mixh( hRule, m.x ), m.y ), m.z );
					hRed = mixh( mixh( mixh( hRed, r.x ), r.y ), r.z );
				}
			}
		std::printf( "   openprobe: below hits %ld, below misses %ld (returning sky %ld), above misses %ld, above hash rule %016llx red %016llx\n",
			belowHit, belowMiss, belowMissSky, aboveMiss, (unsigned long long)hRule, (unsigned long long)hRed );
		std::printf( "   openprobe: below-miss mean green radiance rule %.4f vs red %.4f\n", belowMiss ? lowRule / belowMiss : 0.0,
			belowMiss ? lowRed / belowMiss : 0.0 );
		report( "openprobe", belowMiss > 0 && belowMissSky == 0 && hRule == hRed,
			"below-horizon misses returning sky %.0f of %.0f, above hash equal", double( belowMissSky ), double( belowMiss ) );
	}
	if ( g_fail ) {
		std::printf( "SKYBELOW2 FAIL %d\n", g_fail );
		return 1;
	}
	std::printf( "SKYBELOW2 PASS 6\n" );
	return 0;
}
