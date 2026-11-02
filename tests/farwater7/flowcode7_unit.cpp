// lane FARWATER7 (FARWATER7-SAT): flow encoding 2, the three-quarter-octave log speed nibble (src/lodtflowcode.h),
// compiled alone.
//   flowcode7_unit <ratios.f64> <codes.out>   writes, per ratio, the encoding 0, 1 and 2 codes (3 bytes)
//   flowcode7_unit --self                      the decode checks; prints FLOWCODE7 SELF PASS|FAIL
#include "../../src/lodtflowcode.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int self()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	int bad = 0;
	// encoding 2: code 8 = 1x, 0 = still, 15 = 2^5.25, every code round-trips
	if ( lodtFlowSpeedFactor( kLodlFlowEncLog34, 8 ) != 1.0 || lodtFlowSpeedFactor( kLodlFlowEncLog34, 0 ) != 0.0
		|| lodtFlowSpeedFactor( kLodlFlowEncLog34, 15 ) != std::exp2( 5.25 ) ) {
		std::printf( "FAIL encoding 2 anchors\n" );
		bad++;
	}
	for ( int c = 1; c < 16; c++ )
		if ( lodtFlowSpeedCode( kLodlFlowEncLog34, lodtFlowSpeedFactor( kLodlFlowEncLog34, c ) ) != c ) {
			std::printf( "FAIL encoding 2 round trip code %d\n", c );
			bad++;
		}
	// inside the range (codes 0.5 .. 15.5 = 2^-5.625 .. 2^5.625) the decode is within 3/8 octave of the truth
	double worst = 0.0;
	for ( double l = -5.62; l <= 5.62; l += 0.001 ) {
		const double r = std::exp2( l );
		const double q = lodtFlowSpeedFactor( kLodlFlowEncLog34, lodtFlowSpeedCode( kLodlFlowEncLog34, r ) ) / r;
		const double e = std::fabs( std::log2( q ) );
		worst = e > worst ? e : worst;
	}
	std::printf( "encoding 2: worst decode error inside 2^+-5.62 = %.4f octave (bar 0.375)\n", worst );
	if ( worst > 0.375 + 1e-9 )
		bad++;
	// the red, encoding 1, over the same range: its 15 stops at 11.3x (worst error must exceed the bar)
	double worst1 = 0.0;
	for ( double l = -5.62; l <= 5.62; l += 0.001 ) {
		const double r = std::exp2( l );
		const double q = lodtFlowSpeedFactor( kLodlFlowEncLog, lodtFlowSpeedCode( kLodlFlowEncLog, r ) ) / r;
		const double e = std::fabs( std::log2( q ) );
		worst1 = e > worst1 ? e : worst1;
	}
	std::printf( "red encoding 1 over the same range: worst %.4f octave -> %s\n", worst1,
		worst1 > 0.375 + 1e-9 ? "FAILS the bar (red ok)" : "PASSES (red not red)" );
	if ( !( worst1 > 0.375 + 1e-9 ) )
		bad++;
	// encodings 0 and 1 are the FARWATER5 rules bit for bit (the refactor through lodtFlowSpeedCodeExact)
	int moved = 0;
	unsigned seed = 777u;
	auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return double( seed >> 8 ) / 16777216.0; };
	for ( int t = 0; t < 200000; t++ ) {
		const double r = std::exp2( rnd() * 24.0 - 12.0 );
		double c1 = std::floor( 8.0 + 2.0 * std::log2( r ) + 0.5 ), c0 = std::floor( 8.0 * r + 0.5 );
		const int o1 = c1 < 0.0 ? 0 : c1 > 15.0 ? 15 : int( c1 ), o0 = c0 < 0.0 ? 0 : c0 > 15.0 ? 15 : int( c0 );
		moved += lodtFlowSpeedCode( kLodlFlowEncLog, r ) != o1;
		moved += lodtFlowSpeedCode( kLodlFlowEncLinear, r ) != o0;
	}
	for ( int c = 0; c < 16; c++ ) {
		const double a = lodtFlowSpeedFactor( kLodlFlowEncLog, c ), b = c <= 0 ? 0.0 : std::exp2( double( c - 8 ) * 0.5 );
		moved += std::memcmp( &a, &b, sizeof a ) != 0;
	}
	std::printf( "encodings 0 / 1 vs the FARWATER5 inline rules: %d of 400016 differ\n", moved );
	if ( moved )
		bad++;
	// known encodings; an unknown one still reads as linear
	if ( !lodtFlowEncodingKnown( 0u ) || !lodtFlowEncodingKnown( 1u ) || !lodtFlowEncodingKnown( 2u )
		|| lodtFlowEncodingKnown( 3u ) || lodtFlowSpeedFactor( 3u, 5 ) != lodtFlowSpeedFactor( kLodlFlowEncLinear, 5 ) ) {
		std::printf( "FAIL encoding ids\n" );
		bad++;
	}
	std::printf( "FLOWCODE7 SELF %s\n", bad ? "FAIL" : "PASS" );
	return bad ? 1 : 0;
}

int main( int argc, char ** argv )
{
	if ( argc == 2 && !std::strcmp( argv[1], "--self" ) )
		return self();
	if ( argc != 3 ) {
		std::fprintf( stderr, "usage: flowcode7_unit <ratios.f64> <codes.out> | --self\n" );
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
	std::vector<unsigned char> out( r.size() * 3 );
	for ( size_t i = 0; i < r.size(); i++ ) {
		out[3 * i] = (unsigned char) lodtFlowSpeedCode( kLodlFlowEncLinear, r[i] );
		out[3 * i + 1] = (unsigned char) lodtFlowSpeedCode( kLodlFlowEncLog, r[i] );
		out[3 * i + 2] = (unsigned char) lodtFlowSpeedCode( kLodlFlowEncLog34, r[i] );
	}
	FILE * o = std::fopen( argv[2], "wb" );
	if ( !o )
		return 2;
	std::fwrite( out.data(), 1, out.size(), o );
	std::fclose( o );
	std::printf( "%zu ratios coded\n", r.size() );
	return 0;
}
