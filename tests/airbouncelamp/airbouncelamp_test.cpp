// lane AIRBOUNCE1 (AIR-LAMPS-BOUNCE): src/gl/airbouncelamp.h against a Monte Carlo of a point lamp over a Lambertian floor
// build: g++ -std=c++17 -O2 -I<tree>/src tests/airbouncelamp/airbouncelamp_test.cpp -o airbouncelamp_test
// run:   airbouncelamp_test <tree>   (reads tests/airbouncelamp/airbouncelamp_mc.tsv and res/shaders/cell_volair.frag)
// red:   add -DAIRBOUNCELAMP_RED (no bounce: AIRMS1's air only): (b), (c) and (e) must FAIL
// bars written before the first run, at floor albedos 0.1, 0.2, 0.4: (b) total (single + AIRMS1's rest + bounce) rms
// log err <= 0.20 (the fit: 0.135, without the bounce 0.40); (c) every albedo's mean log err within 0.15;
// (e) the bounce alone against the MC's ground share, rms log err <= 0.45 (the fit: 0.34)
#include "gl/airbouncelamp.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct Row
{
	std::string phase, view;
	airmslamp::Lobes p;
	float sigma, hl, rho, hx, d, tau, m1, c0, c1, c2, c3, se;
};

static int fails = 0;
static void check( bool ok, const char * what )
{
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
	if ( !ok )
		fails++;
}

// the MC's frame: lamp (0, 0, hl), x (rho, 0, hx), floor z 0; into = x -> lamp, side = into x (0, 1, 0), away = -into
static void frame( const Row & r, float L[3], float x[3], float dW[3], float & c )
{
	L[0] = 0.0f, L[1] = 0.0f, L[2] = r.hl;
	x[0] = r.rho, x[1] = 0.0f, x[2] = r.hx;
	float u[3] = { -r.rho, 0.0f, r.hl - r.hx };
	float n = std::sqrt( u[0] * u[0] + u[2] * u[2] );
	u[0] /= n, u[2] /= n;
	if ( r.view == "into" )
		dW[0] = u[0], dW[1] = 0.0f, dW[2] = u[2];
	else if ( r.view == "away" )
		dW[0] = -u[0], dW[1] = 0.0f, dW[2] = -u[2];
	else	// u x (0, 1, 0) = (-u.z, 0, u.x)
		dW[0] = -u[2], dW[1] = 0.0f, dW[2] = u[0];
	c = u[0] * dW[0] + u[2] * dW[2];
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string root = argc > 1 ? argv[1] : ".";
	std::ifstream f( root + "/tests/airbouncelamp/airbouncelamp_mc.tsv" );
	std::vector<Row> rows;
	for ( std::string line; std::getline( f, line ); ) {
		if ( line.empty() || line[0] == '#' )
			continue;
		std::istringstream s( line );
		Row r;
		s >> r.phase >> r.view >> r.p.wAir >> r.p.wFwd >> r.p.gFwd >> r.p.wBack >> r.p.gBack >> r.sigma >> r.hl >> r.rho >> r.hx
			>> r.d >> r.tau >> r.m1 >> r.c0 >> r.c1 >> r.c2 >> r.c3 >> r.se;
		if ( s )
			rows.push_back( r );
	}
	check( rows.size() == 966, "fixture: 966 MC rows" );

	// (a) off = 0 exactly; on >= 0; nothing under the floor
	bool zero = true, pos = true;
	for ( const Row & r : rows ) {
		float L[3], x[3], dW[3], c;
		frame( r, L, x, dW, c );
		zero = zero && airbouncelamp::lampGnd( r.p, L, 0.0f, x, dW, r.d, r.tau, 0.2f, false ) == 0.0f;
		pos = pos && airbouncelamp::lampGnd( r.p, L, 0.0f, x, dW, r.d, r.tau, 0.2f, true ) >= 0.0f;
	}
	{
		Row r = rows[0];
		float L[3], x[3], dW[3], c;
		frame( r, L, x, dW, c );
		zero = zero && airbouncelamp::lampGnd( r.p, L, r.hl + 1.0f, x, dW, r.d, r.tau, 0.2f, true ) == 0.0f;	// lamp under it
		zero = zero && airbouncelamp::lampGnd( r.p, L, r.hx + 0.5f, x, dW, r.d, r.tau, 0.2f, true ) == 0.0f;	// x under it
	}
	check( zero, "(a) off == 0 exactly, and 0 with the lamp or x under the floor" );
	check( pos, "(a) on >= 0 on every row" );

	// (b) (c) (e) against the MC at three floor albedos
	const float alb[3] = { 0.1f, 0.2f, 0.4f };
	double e2 = 0.0, emax = 0.0, g2 = 0.0;
	double mean[3] = { 0.0, 0.0, 0.0 };
	int n = 0, ng = 0;
	for ( int ia = 0; ia < 3; ia++ ) {
		const double a = alb[ia];
		for ( const Row & r : rows ) {
			float L[3], x[3], dW[3], c;
			frame( r, L, x, dW, c );
			double gnd = a * r.c1 + a * a * r.c2 + a * a * a * r.c3;
			double mc = double( r.m1 ) + r.c0 + gnd;
			double b = airbouncelamp::lampGnd( r.p, L, 0.0f, x, dW, r.d, r.tau, alb[ia], true ) / airmslamp::kPi;
			double law = airmslamp::lampPh( r.p, c, r.tau, true ) / airmslamp::kPi + b;
			double e = std::log( mc / law );
			e2 += e * e;
			emax = std::fmax( emax, std::fabs( e ) );
			mean[ia] += e;
			n++;
			if ( gnd > 0.0 ) {
				double eg = b > 0.0 ? std::log( gnd / b ) : 30.0;
				g2 += eg * eg;
				ng++;
			}
		}
		mean[ia] /= double( rows.size() );
	}
	double rms = std::sqrt( e2 / double( n ) ), rmsg = std::sqrt( g2 / double( ng ) );
	std::printf( "     total rms log err %.3f  max %.3f\n", rms, emax );
	check( rms <= 0.20, "(b) total rms log err <= 0.20" );
	bool unbiased = true;
	for ( int ia = 0; ia < 3; ia++ ) {
		std::printf( "     albedo %.1f mean log err %+.3f\n", double( alb[ia] ), mean[ia] );
		unbiased = unbiased && std::fabs( mean[ia] ) <= 0.15;
	}
	check( unbiased, "(c) every albedo's mean log err within 0.15" );

	// (d) the shader carries the same law at both lamp sites
	std::ifstream sh( root + "/res/shaders/cell_volair.frag", std::ios::binary );
	std::string src( ( std::istreambuf_iterator<char>( sh ) ), std::istreambuf_iterator<char>() );
	auto count = [&src]( const std::string & k ) {
		int c = 0;
		for ( size_t p = src.find( k ); p != std::string::npos; p = src.find( k, p + 1 ) )
			c++;
		return c;
	};
	check( count( "e * airLampGnd( t0.xyz, hf, Pk, dW, d, od )" ) == 2, "(d) shader: the term at both lamp sites (airRest, airLightsQ)" );
	check( count( "volGndLamp.x * ( 1.0 + volGndLamp.y * ( od / d ) * D ) * 2.0 * ( ( hx + hl ) / D ) * ph * ( d * d / D2 )" ) == 1
			&& count( "volHg( c, volPhase[1].w * volGndLamp.z ) + volPhase[2].rgb * volHg( c, volPhase[2].w * volGndLamp.z )" ) == 1
			&& count( "vec3 v = vec3( L.xy, hf - hl ) - Pk;" ) == 1,
		"(d) shader: airLampGnd is the twin's law" );

	std::printf( "     bounce alone vs the MC ground: rms log err %.3f (%d rows)\n", rmsg, ng );
	check( rmsg <= 0.45, "(e) the bounce alone within rms log 0.45 of the MC's ground share" );
	std::printf( "%s (%d failed)\n", fails ? "FAILED" : "PASSED", fails );
	return fails ? 1 : 0;
}
