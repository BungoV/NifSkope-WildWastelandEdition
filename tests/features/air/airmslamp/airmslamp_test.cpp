// lane AIRMS1 (AIR-MS-LAMPS): src/features/air/airmslamp.h against a Monte Carlo of a point lamp in the cell air
// build: g++ -std=c++17 -O2 -I<tree>/src tests/features/air/airmslamp/airmslamp_test.cpp -o airmslamp_test
// run:   airmslamp_test <tree>   (reads tests/features/air/airmslamp/airmslamp_mc.tsv and res/shaders/features/air/cell_volair.frag)
// red:   add -DAIRMSLAMP_RED (no lamp term, today's air): (b) and (c) must FAIL
// bars written before the first run: (b) total rms log err <= 0.20 (today 0.98), (c) per phase |mean log err| <= 0.35
#include "features/air/airmslamp.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Row
{
	std::string phase, view;
	airmslamp::Lobes p;
	float tau, c, m1, mair, se;
};

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string root = argc > 1 ? argv[1] : ".";
	std::ifstream f( root + "/tests/features/air/airmslamp/airmslamp_mc.tsv" );
	std::vector<Row> rows;
	for ( std::string line; std::getline( f, line ); ) {
		if ( line.empty() || line[0] == '#' )
			continue;
		std::istringstream s( line );
		Row r;
		s >> r.phase >> r.view >> r.p.wAir >> r.p.wFwd >> r.p.gFwd >> r.p.wBack >> r.p.gBack >> r.tau >> r.c >> r.m1 >> r.mair >> r.se;
		if ( s )
			rows.push_back( r );
	}
	check( rows.size() == 495, "fixture: 495 MC rows" );

	// (a) off = today's single scattering, bit for bit; on never darker
	bool same = true, brighter = true;
	for ( const Row & r : rows ) {
		float off = airmslamp::lampPh( r.p, r.c, r.tau, false );
		float today = std::exp( -r.tau ) * airmslamp::phasePi( r.p, r.c, 1.0f );
		same = same && off == today;
		brighter = brighter && airmslamp::lampPh( r.p, r.c, r.tau, true ) >= off;
	}
	check( same, "(a) off == exp(-od) pi p(c) exactly" );
	check( brighter, "(a) on >= off on every row" );

	// (b) single + rest against the MC's total
	double e2 = 0.0, emax = 0.0;
	std::map<std::string, std::pair<double, int>> per;
	for ( const Row & r : rows ) {
		double mc = airmslamp::kPi * ( double( r.m1 ) + double( r.mair ) );
		double law = airmslamp::lampPh( r.p, r.c, r.tau, true );
		double e = std::log( mc / law );
		e2 += e * e;
		emax = std::fmax( emax, std::fabs( e ) );
		per[r.phase].first += e;
		per[r.phase].second++;
	}
	double rms = std::sqrt( e2 / double( rows.size() ) );
	std::printf( "     total rms log err %.3f  max %.3f\n", rms, emax );
	check( rms <= 0.20, "(b) total rms log err <= 0.20" );

	// (c) no phase biased
	bool unbiased = true;
	for ( auto & kv : per ) {
		double m = kv.second.first / kv.second.second;
		std::printf( "     %-11s mean log err %+.3f\n", kv.first.c_str(), m );
		unbiased = unbiased && std::fabs( m ) <= 0.35;
	}
	check( unbiased, "(c) every phase's mean log err within 0.35" );

	// (d) the shader carries the same law at both lamp sites
	std::ifstream sh( root + "/res/shaders/features/air/cell_volair.frag", std::ios::binary );
	std::string src( ( std::istreambuf_iterator<char>( sh ) ), std::istreambuf_iterator<char>() );
	auto count = [&src]( const std::string & k ) {
		int n = 0;
		for ( size_t p = src.find( k ); p != std::string::npos; p = src.find( k, p + 1 ) )
			n++;
		return n;
	};
	check( count( "e * airLampMs( c, od )" ) == 2, "(d) shader: the term at both lamp sites (airRest, airLightsQ)" );
	check( count( "volHg( c, volPhase[1].w * s ) + volPhase[2].rgb * volHg( c, volPhase[2].w * s )" ) == 1
			&& count( "max( volMsLamp.x + volMsLamp.y * volGi.w, 0.0 ) * od * exp( -volMsLamp.z * od ) * ph" ) == 1,
		"(d) shader: airLampMs is the twin's law" );
	std::printf( "%s (%d failed)\n", fails ? "FAILED" : "PASSED", fails );
	return fails ? 1 : 0;
}
