// lane LIGHTNING2: the lightning law's CPU side (src/gl/cloudlightning.h) for tests/spells/lightning2_check.py; no GL, no Qt.
//   lightning2_dump sched <law 0|1> <byte> <share> <seed> <t1 s>
//       the schedule over [0, t1): strikes, the measured mean gap, the schedule's meanIntervalS, the longest flash, flashes
//       that leave their slot, the gain; law 0 = CLOUD6f's schedule (oldSchedule), 1 = the DATA law (freqSchedule)
//   lightning2_dump oldsame <seed> <slots>   the old wrappers (strikeOf(k, seed), levelAt, nextStrikeS) == the Schedule
//       forms with oldSchedule, bit for bit, over that many slots and 1 ms samples of the first 400 s
//   lightning2_dump ground <sigma> <w> <g> <base> <top> <sx> <sy> <sz> <gx> <gy> <gz>
//       the Field constants and groundDalc's six values
//   lightning2_dump field ...   as cloudlightning_dump field (fieldc line, then one fieldAt per stdin line)
#include "gl/cloudlightning.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace L = WwCloudLightning;

static int sched( int argc, char ** argv )
{
	if ( argc < 7 )
		return 2;
	const bool law = std::atoi( argv[2] ) != 0;
	const unsigned byte = unsigned( std::atoi( argv[3] ) );
	const float share = float( std::atof( argv[4] ) );
	const std::uint32_t seed = std::uint32_t( std::strtoul( argv[5], nullptr, 0 ) );
	const double t1 = std::atof( argv[6] );
	const L::Schedule sc = law ? L::freqSchedule( share, byte ) : L::oldSchedule( share );
	int strikes = 0, cross = 0;
	double prev = -1.0, gaps = 0.0, maxLen = 0.0;
	for ( std::int64_t k = 0; double( k ) * sc.slotS < t1; k++ ) {
		const L::Strike s = L::strikeOf( k, seed, sc );
		if ( !s.on )
			continue;
		strikes++;
		if ( prev >= 0.0 )
			gaps += s.startS - prev;
		prev = s.startS;
		const double end = s.strokeS[s.strokes - 1] + L::kStrokeLenS;
		maxLen = std::max( maxLen, end - s.startS );
		if ( L::slotOf( s.startS, sc.slotS ) != k || L::slotOf( end, sc.slotS ) != k )
			cross++;
	}
	// the level is 0 between flashes, exactly, and the next-strike search finds the first strike after 0
	long long lit = 0, n = 0;
	for ( double t = 0.0; t < std::min( t1, 600.0 ); t += 0.001, n++ )
		lit += L::levelAt( t, seed, sc, nullptr ) > 0.0f ? 1 : 0;
	std::printf( "sched law %d byte %u share %.4f seed %u t1 %.0f slot %.3f chance %.6f gain %.4f strikes %d meangap %.4f "
				 "every %.4f maxlen %.4f cross %d duty %.6f next0 %.4f\n", law ? 1 : 0, byte, double( share ), seed, t1,
		sc.slotS, double( sc.chance ), double( sc.gain ), strikes, strikes > 1 ? gaps / double( strikes - 1 ) : -1.0,
		L::meanIntervalS( sc ), maxLen, cross, n ? double( lit ) / double( n ) : 0.0, L::nextStrikeS( 0.0, seed, sc ) );
	return 0;
}

static int oldsame( int argc, char ** argv )
{
	if ( argc < 4 )
		return 2;
	const std::uint32_t seed = std::uint32_t( std::strtoul( argv[2], nullptr, 0 ) );
	const int slots = std::atoi( argv[3] );
	const L::Schedule sc = L::oldSchedule( 1.0f );
	long long diff = 0, checked = 0;
	for ( int k = 0; k < slots; k++ ) {
		const L::Strike a = L::strikeOf( k, seed ), b = L::strikeOf( k, seed, sc );
		checked++;
		if ( a.on != b.on || std::memcmp( &a.startS, &b.startS, sizeof a.startS ) || a.strokes != b.strokes
			|| std::memcmp( a.strokeS, b.strokeS, sizeof a.strokeS ) || std::memcmp( a.amp, b.amp, sizeof a.amp )
			|| std::memcmp( a.offKm, b.offKm, sizeof a.offKm ) )
			diff++;
	}
	for ( double t = 0.0; t < 400.0; t += 0.001 ) {
		const float a = L::levelAt( t, seed ), b = L::levelAt( t, seed, sc, nullptr );
		const double na = L::nextStrikeS( t, seed ), nb = L::nextStrikeS( t, seed, sc );
		checked++;
		if ( std::memcmp( &a, &b, sizeof a ) || std::memcmp( &na, &nb, sizeof na ) )
			diff++;
	}
	std::printf( "oldsame seed %u checked %lld differ %lld\n", seed, checked, diff );
	return 0;
}

static L::Field fieldArgs( char ** argv, int i0, float & sigma, float & base )
{
	sigma = float( std::atof( argv[i0] ) );
	const float w = float( std::atof( argv[i0 + 1] ) ), g = float( std::atof( argv[i0 + 2] ) );
	base = float( std::atof( argv[i0 + 3] ) );
	const float top = float( std::atof( argv[i0 + 4] ) );
	const float src[3] = { float( std::atof( argv[i0 + 5] ) ), float( std::atof( argv[i0 + 6] ) ), float( std::atof( argv[i0 + 7] ) ) };
	return L::fieldOf( sigma, w, g, base, top, src, true );
}

static void printField( const L::Field & f, float base, float sigma, float sunK )
{
	std::printf( "fieldc %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", double( f.src[0] ), double( f.src[1] ),
		double( f.src[2] ), double( f.invDelta ), double( f.r0sq ), double( f.mirLo ), double( f.mirHi ), double( f.flux ),
		double( f.norm ), double( L::powerFor( f, base, sigma, sunK ) ) );
}

static int ground( int argc, char ** argv )
{
	if ( argc < 13 )
		return 2;
	float sigma = 0.0f, base = 0.0f;
	const L::Field f = fieldArgs( argv, 2, sigma, base );
	const float gp[3] = { float( std::atof( argv[10] ) ), float( std::atof( argv[11] ) ), float( std::atof( argv[12] ) ) };
	printField( f, base, sigma, 1.0f );
	float e[6];
	L::groundDalc( f, base, sigma, gp, e );
	std::printf( "ground %.9g %.9g %.9g %.9g %.9g %.9g deck %.9g\n", double( e[0] ), double( e[1] ), double( e[2] ),
		double( e[3] ), double( e[4] ), double( e[5] ), double( L::deckKm( base, sigma ) ) );
	return 0;
}

static int field( int argc, char ** argv )
{
	if ( argc < 11 )
		return 2;
	float sigma = 0.0f, base = 0.0f;
	const L::Field f = fieldArgs( argv, 2, sigma, base );
	printField( f, base, sigma, float( std::atof( argv[10] ) ) );
	float p[3], d[3];
	while ( std::scanf( "%f %f %f %f %f %f", &p[0], &p[1], &p[2], &d[0], &d[1], &d[2] ) == 6 )
		std::printf( "f %.9g\n", double( L::fieldAt( f, p, d ) ) );
	return 0;
}

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		return 2;
	if ( !std::strcmp( argv[1], "sched" ) )
		return sched( argc, argv );
	if ( !std::strcmp( argv[1], "oldsame" ) )
		return oldsame( argc, argv );
	if ( !std::strcmp( argv[1], "ground" ) )
		return ground( argc, argv );
	if ( !std::strcmp( argv[1], "field" ) )
		return field( argc, argv );
	return 2;
}
