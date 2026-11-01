// lane FARLOOK1101 (FARWATER5 L3): the speed nibble's two encodings (src/lodtflowcode.h), compiled alone.
//   flowcode_unit <ratios.f64> <codes.out>   writes, per ratio, the linear code then the log code (2 bytes)
//   flowcode_unit --self                      the decode checks; prints FLOWCODE SELF PASS|FAIL
#include "../../src/lodtflowcode.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int self()
{
	int bad = 0;
	// linear decode x reference == the old reader's code * reference / 8, bit for bit
	const double refs[] = { 0.0137, 1.0, 3.3333333333, 27.8, 1234.5678, 1e-7, 6.02e5 };
	for ( double m : refs )
		for ( int c = 0; c < 16; c++ ) {
			const double a = lodtFlowSpeedFactor( kLodlFlowEncLinear, c ) * m, b = double( c ) * m / 8.0;
			if ( std::memcmp( &a, &b, sizeof a ) ) {
				std::printf( "FAIL linear decode c %d m %g: %.17g vs %.17g\n", c, m, a, b );
				bad++;
			}
		}
	// an unknown encoding reads as linear (an old reader's behaviour)
	for ( int c = 0; c < 16; c++ )
		if ( lodtFlowSpeedFactor( 7u, c ) != lodtFlowSpeedFactor( kLodlFlowEncLinear, c ) )
			bad++;
	// log: code 8 = 1x, every code round-trips, 0 = still, decode(code(r)) within a quarter octave inside the range
	if ( lodtFlowSpeedFactor( kLodlFlowEncLog, 8 ) != 1.0 || lodtFlowSpeedFactor( kLodlFlowEncLog, 0 ) != 0.0 )
		bad++;
	for ( int c = 1; c < 16; c++ )
		if ( lodtFlowSpeedCode( kLodlFlowEncLog, lodtFlowSpeedFactor( kLodlFlowEncLog, c ) ) != c ) {
			std::printf( "FAIL log round trip code %d\n", c );
			bad++;
		}
	double worst = 0.0;
	for ( double l = -3.74; l <= 3.74; l += 0.001 ) {
		const double r = std::exp2( l );
		const double q = lodtFlowSpeedFactor( kLodlFlowEncLog, lodtFlowSpeedCode( kLodlFlowEncLog, r ) ) / r;
		const double e = q > 1.0 ? q - 1.0 : 1.0 / q - 1.0;
		worst = e > worst ? e : worst;
	}
	const double lim = std::exp2( 0.25 ) - 1.0 + 1e-9;
	std::printf( "log nibble: worst ratio error inside 2^+-3.74 = %.4f (bar %.4f, a quarter octave)\n", worst, lim );
	if ( worst > lim )
		bad++;
	// linear red over the same range: it cannot carry 1.94x and up (worst error must exceed the bar)
	double worstLin = 0.0;
	for ( double l = -3.74; l <= 3.74; l += 0.001 ) {
		const double r = std::exp2( l );
		const double q = lodtFlowSpeedFactor( kLodlFlowEncLinear, lodtFlowSpeedCode( kLodlFlowEncLinear, r ) ) / r;
		const double e = q <= 0.0 ? 1e9 : q > 1.0 ? q - 1.0 : 1.0 / q - 1.0;
		worstLin = e > worstLin ? e : worstLin;
	}
	std::printf( "red linear nibble: worst ratio error over the same range = %.4g -> %s\n", worstLin,
		worstLin > lim ? "FAILS the bar" : "PASSES (red not red)" );
	if ( !( worstLin > lim ) )
		bad++;
	std::printf( "FLOWCODE SELF %s\n", bad ? "FAIL" : "PASS" );
	return bad ? 1 : 0;
}

int main( int argc, char ** argv )
{
	if ( argc == 2 && !std::strcmp( argv[1], "--self" ) )
		return self();
	if ( argc != 3 ) {
		std::fprintf( stderr, "usage: flowcode_unit <ratios.f64> <codes.out> | --self\n" );
		return 2;
	}
	FILE * f = std::fopen( argv[1], "rb" );
	if ( !f )
		return 2;
	std::vector<double> r;
	double v;
	while ( std::fread( &v, sizeof v, 1, f ) == 1 )
		r.push_back( v );
	std::fclose( f );
	std::vector<unsigned char> out( r.size() * 2 );
	for ( size_t i = 0; i < r.size(); i++ ) {
		out[2 * i] = (unsigned char) lodtFlowSpeedCode( kLodlFlowEncLinear, r[i] );
		out[2 * i + 1] = (unsigned char) lodtFlowSpeedCode( kLodlFlowEncLog, r[i] );
	}
	FILE * o = std::fopen( argv[2], "wb" );
	if ( !o )
		return 2;
	std::fwrite( out.data(), 1, out.size(), o );
	std::fclose( o );
	std::printf( "%zu ratios coded\n", r.size() );
	return 0;
}
