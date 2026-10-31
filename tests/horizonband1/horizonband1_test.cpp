// Lane HORIZONBAND1 (FIX1031B, 2026-10-31): the sky's horizon band (res/shaders/atmo_skyband.glsl, compiled here as
// written through glsl_shim_hb.h) on the measured frame of bungo's defect (notes/fix1031b/h1, approved spot 3,
// concord_hatch_boston.path, CommonwealthClear 12:00, exe main 336de9dc).
//
// Fixture c3_column.txt (notes/fix1031b/sky/tools/mk_fixture.py): per picture row of columns 100 and 490, the row's
// elevation (the path log's camera), the air's T along the sky ray (tFog: the c3_hb_base WW_CELL_AIR_DUMP's medium,
// wwVolAirSky's froxels + tail) and the sky's own T (tSky: tests/spells/atmo_model.py, haze 0.513/km @ 0.214 km,
// camera 134 m); the backdrop as displayed (c3_hb_nosky: the air over the sky off) and today's picture (c3_hb_base).
// Per column: the fog's colour at infinity as displayed (today's strip plateau, cover 1), the air's horizon colour
// (proxy: c3_hb_noair's far terrain = the aerial perspective alone, REASONED), the silhouette's AA row, the terrain.
// Display composite (cell_hdr.frag mode 2): out = cover colour x (1 - T') + backdrop x T'; the cover's colour mix is
// taken in display space (the tone map's curve over a ~1 deg band: an approximation, said in the output).
// Checks (bars written before the first green run):
//   step    the largest one-row change (max channel, 8-bit levels) of the sky rows from +2 deg down to the silhouette:
//           <= 5 (today's twin 8.7 / 10.6; the picture 13.7 .. 22.3). The red fails it.
//   cyan    the cover's b - r at the band's base (the last sky row) <= 60 (today 92 / 86: the saturated cyan). Red fails.
//   meet    the step from the last sky row into the terrain <= 15 and <= 0.6 x today's (proxy colour, REASONED).
//   open    rows from +2.5 deg up are today's to 0.5 levels (the open sky is untouched).
//   foggy   a fog that leads high up (tFog 0.01, tSky 0.5) keeps today's T' and light exactly.
// Red: -DHORIZONBAND1_RED runs rule 0 (today's clamp, WW_ATMO_AP_RED=skyclamp) everywhere: step, cyan, meet must FAIL.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace glsl
{
#include "glsl_shim_hb.h"
#include "../../res/shaders/atmo_skyband.glsl"
}

struct Row
{
	int y = 0;
	double e = 0, tFog = 0, tSky = 0, B[3] = {}, base[3] = {};
};
struct Col
{
	int edge = 0, aa = 0, terr = 0;
	double fi[3] = {}, hz[3] = {};
	std::vector<Row> rows;
};

static int fails = 0;
static void check( bool ok, const char * name, const std::string & what )
{
	std::printf( "%s %s: %s\n", ok ? "PASS" : "FAIL", name, what.c_str() );
	if ( !ok )
		fails++;
}

#ifdef HORIZONBAND1_RED
static const int kRule = 0;
#else
static const int kRule = 1;
#endif

// the display composite of one sky row under a rule: cover colour (display) x (1 - T') + backdrop x T'
static void composite( const Col & c, const Row & r, int rule, double out[3] )
{
	const float tr = glsl::wwSkyBandT( float( r.tFog ), float( r.tSky ), rule );
	// the cover's colour mix runs through the shader's own wwSkyBandL: fogL = fogInf x (1 - tFog), apL = hz x (1 - tAp)
	const double tAp = 0.5, tf = std::min( r.tFog, 0.999 );
	for ( int k = 0; k < 3; k++ ) {
		const glsl::vec3 fogL( c.fi[k] * ( 1.0 - tf ), 0.0, 0.0 ), apL( c.hz[k] * ( 1.0 - tAp ), 0.0, 0.0 );
		const glsl::vec3 L = glsl::wwSkyBandL( fogL, float( tf ), apL, float( tAp ), float( r.tSky ), tr, rule );
		out[k] = double( L.x ) + r.B[k] * double( tr );
	}
}

struct Series
{
	double step = 0, stepE = 0, meet = 0, baseBR = 0;
	std::vector<std::pair<int, std::vector<double>>> sky;
};

static Series run( const Col & c, int rule )
{
	Series s;
	std::vector<double> prev;
	double prevE = 0;
	for ( const Row & r : c.rows ) {
		if ( r.y == c.aa || r.y >= c.terr || r.e > 2.0 )
			continue;
		double o[3];
		composite( c, r, rule, o );
		std::vector<double> v( o, o + 3 );
		if ( !prev.empty() ) {
			double d = 0;
			for ( int k = 0; k < 3; k++ )
				d = std::max( d, std::fabs( v[k] - prev[k] ) );
			if ( d > s.step ) {
				s.step = d;
				s.stepE = prevE;
			}
		}
		prev = v;
		prevE = r.e;
		s.sky.push_back( { r.y, v } );
	}
	for ( const Row & r : c.rows )
		if ( r.y == c.terr && !prev.empty() ) {
			for ( int k = 0; k < 3; k++ )
				s.meet = std::max( s.meet, std::fabs( r.base[k] - prev[k] ) );
			s.baseBR = prev[2] - prev[0];
		}
	return s;
}

int main( int argc, char ** argv )
{
	const char * fx = argc > 1 ? argv[1] : "tests/horizonband1/c3_column.txt";
	std::ifstream in( fx );
	if ( !in ) {
		std::printf( "FAIL fixture: cannot read %s\n", fx );
		return 2;
	}
	std::map<int, Col> cols;
	std::string line;
	while ( std::getline( in, line ) ) {
		std::istringstream ss( line );
		std::string w;
		ss >> w;
		if ( w == "column" ) {
			int x;
			std::string t;
			Col c;
			ss >> x >> t >> c.edge >> t >> c.aa >> t >> c.terr >> t >> c.fi[0] >> c.fi[1] >> c.fi[2] >> t >> c.hz[0] >> c.hz[1] >> c.hz[2];
			cols[x] = c;
		} else if ( !w.empty() ) {
			Row r;
			const int x = std::atoi( w.c_str() );
			ss >> r.y >> r.e >> r.tFog >> r.tSky >> r.B[0] >> r.B[1] >> r.B[2] >> r.base[0] >> r.base[1] >> r.base[2];
			cols[x].rows.push_back( r );
		}
	}
	if ( cols.size() < 2 ) {
		std::printf( "FAIL fixture: %zu columns\n", cols.size() );
		return 2;
	}
	std::printf( "HORIZONBAND1 rule %d (%s); display-space mix of the cover colour (approximation)\n", kRule,
		kRule ? "the horizon band" : "RED: today's clamp" );
	char b[256];
	for ( const auto & kv : cols ) {
		const Col & c = kv.second;
		const Series now = run( c, kRule ), today = run( c, 0 );
		std::snprintf( b, sizeof( b ), "x %d max one-row step %.1f levels at %+.2f deg (today %.1f at %+.2f), bar <= 5",
			kv.first, now.step, now.stepE, today.step, today.stepE );
		check( now.step <= 5.0, "step", b );
		std::snprintf( b, sizeof( b ), "x %d cover b - r at the band's base %.1f (today %.1f), bar <= 60", kv.first, now.baseBR,
			today.baseBR );
		check( now.baseBR <= 60.0, "cyan", b );
		std::snprintf( b, sizeof( b ), "x %d sky -> terrain step %.1f (today %.1f), bar <= 15 and <= 0.6 x today", kv.first,
			now.meet, today.meet );
		check( now.meet <= 15.0 && now.meet <= 0.6 * today.meet, "meet", b );
		double open = 0;
		int nOpen = 0;
		for ( const Row & r : c.rows ) {
			if ( r.e < 2.5 || r.y >= c.terr )
				continue;
			double o1[3], o0[3];
			composite( c, r, kRule, o1 );
			composite( c, r, 0, o0 );
			for ( int k = 0; k < 3; k++ )
				open = std::max( open, std::fabs( o1[k] - o0[k] ) );
			nOpen++;
		}
		std::snprintf( b, sizeof( b ), "x %d rows from +2.5 deg (%d): max change %.2f levels, bar <= 0.5", kv.first, nOpen, open );
		check( nOpen > 0 && open <= 0.5, "open", b );
	}
	{	// a fog that leads high up: today's T' and light kept
		const float t1 = glsl::wwSkyBandT( 0.01f, 0.5f, kRule ), t0 = glsl::wwSkyBandT( 0.01f, 0.5f, 0 );
		const glsl::vec3 fl( 0.3, 0.4, 0.5 ), al( 0.2, 0.2, 0.2 );
		const glsl::vec3 l1 = glsl::wwSkyBandL( fl, 0.01f, al, 0.5f, 0.5f, t1, kRule ), l0 = glsl::wwSkyBandL( fl, 0.01f, al, 0.5f, 0.5f, t0, 0 );
		const double d = std::max( std::fabs( double( t1 - t0 ) ), std::max( std::fabs( double( l1.x - l0.x ) ), std::fabs( double( l1.z - l0.z ) ) ) );
		std::snprintf( b, sizeof( b ), "tFog 0.01 tSky 0.5: T' %.4f (today %.4f), light change %.2e, bar 1e-6", t1, t0, d );
		check( d <= 1e-6, "foggy", b );
	}
	if ( fails ) {
		std::printf( "HORIZONBAND1 FAIL %d\n", fails );
		return 1;
	}
	std::printf( "HORIZONBAND1 PASS\n" );
	return 0;
}
