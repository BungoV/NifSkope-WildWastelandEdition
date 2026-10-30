// lane CLOUD6f: the lightning's CPU side (src/gl/cloudlightning.h) for tests/spells/cloudlightning_check.py, no GL, no Qt.
//   cloudlightning_dump timing <seed> <t0 s> <t1 s> <dt s>   the schedule's numbers over [t0, t1)
//   cloudlightning_dump field <sigma> <w> <g> <base> <top> <sx> <sy> <sz> <flux 0|1> <sunK>
//       prints the Field constants and the power, then one value per stdin line "px py pz dx dy dz" (fieldAt)
//   cloudlightning_dump share                                   stormShare over a table of cases
//   cloudlightning_dump switch <values...>                      switchOn per value ("-" = unset)
#include "gl/cloudlightning.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace L = WwCloudLightning;

static int timing( int argc, char ** argv )
{
	if ( argc < 6 )
		return 2;
	const std::uint32_t seed = std::uint32_t( std::strtoul( argv[2], nullptr, 0 ) );
	const double t0 = std::atof( argv[3] ), t1 = std::atof( argv[4] ), dt = std::atof( argv[5] );
	const long long n = (long long)( ( t1 - t0 ) / dt );
	long long lit = 0, outside = 0, cross = 0, nondet = 0;
	double maxLen = 0.0, maxLevel = 0.0;
	int strikes = 0;
	std::vector<double> starts;
	for ( std::int64_t k = L::slotOf( t0 ); double( k ) * L::kSlotS < t1; k++ ) {
		const L::Strike s = L::strikeOf( k, seed );
		if ( !s.on || s.startS < t0 || s.startS >= t1 )
			continue;
		strikes++;
		starts.push_back( s.startS );
		const double end = s.strokeS[s.strokes - 1] + L::kStrokeLenS;
		maxLen = std::max( maxLen, end - s.startS );
		if ( L::slotOf( s.startS ) != k || L::slotOf( end ) != k )
			cross++;
	}
	for ( long long i = 0; i < n; i++ ) {
		const double t = t0 + double( i ) * dt;
		L::Strike s;
		const float l = L::levelAt( t, seed, &s );
		if ( l > 0.0f ) {
			lit++;
			maxLevel = std::max( maxLevel, double( l ) );
			bool in = false;
			for ( int j = 0; j < s.strokes; j++ )
				in = in || ( t >= s.strokeS[j] && t < s.strokeS[j] + L::kStrokeLenS );
			if ( !in )
				outside++;
		}
	}
	// determinism: the same clock read backwards, bit for bit
	for ( long long i = n - 1; i >= 0; i -= 7 ) {
		const double t = t0 + double( i ) * dt;
		const float a = L::levelAt( t, seed ), b = L::levelAt( t, seed );
		if ( std::memcmp( &a, &b, sizeof a ) != 0 )
			nondet++;
	}
	double meanGap = 0.0;
	for ( size_t i = 1; i < starts.size(); i++ )
		meanGap += starts[i] - starts[i - 1];
	meanGap = starts.size() > 1 ? meanGap / double( starts.size() - 1 ) : 0.0;
	std::printf( "timing seed %u samples %lld lit %lld duty %.6f strikes %d meangap %.4f maxlen %.4f maxlevel %.4f "
				 "outside %lld cross %lld nondet %lld\n", seed, n, lit, n ? double( lit ) / double( n ) : 0.0, strikes, meanGap,
		maxLen, maxLevel, outside, cross, nondet );
	for ( int i = 0, k = int( L::slotOf( t0 ) ); i < 6; k++ ) {
		const L::Strike s = L::strikeOf( k, seed );
		if ( !s.on )
			continue;
		std::printf( "strike %.4f strokes %d off %.3f %.3f peakat %.4f next(%.1f) %.4f\n", s.startS, s.strokes,
			double( s.offKm[0] ), double( s.offKm[1] ), s.strokeS[0], s.startS - 1.0, L::nextStrikeS( s.startS - 1.0, seed ) );
		i++;
	}
	return 0;
}

static int field( int argc, char ** argv )
{
	if ( argc < 12 )
		return 2;
	const float sigma = float( std::atof( argv[2] ) ), w = float( std::atof( argv[3] ) ), g = float( std::atof( argv[4] ) );
	const float base = float( std::atof( argv[5] ) ), top = float( std::atof( argv[6] ) );
	const float src[3] = { float( std::atof( argv[7] ) ), float( std::atof( argv[8] ) ), float( std::atof( argv[9] ) ) };
	const bool flux = std::atoi( argv[10] ) != 0;
	const float sunK = float( std::atof( argv[11] ) );
	const L::Field f = L::fieldOf( sigma, w, g, base, top, src, flux );
	std::printf( "fieldc %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", double( f.src[0] ), double( f.src[1] ),
		double( f.src[2] ), double( f.invDelta ), double( f.r0sq ), double( f.mirLo ), double( f.mirHi ), double( f.flux ),
		double( f.norm ), double( L::powerFor( f, base, sigma, sunK ) ) );
	float p[3], d[3];
	while ( std::scanf( "%f %f %f %f %f %f", &p[0], &p[1], &p[2], &d[0], &d[1], &d[2] ) == 6 )
		std::printf( "f %.9g\n", double( L::fieldAt( f, p, d ) ) );
	return 0;
}

static int share()
{
	const float pcts[6] = { 0.0f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f };
	// (stormNow, fadeIn, changing, stormLast, fadeOut)
	struct C { int now; float fi; int ch; int last; float fo; } cs[6] = { { 1, 0.5f, 0, 0, 1.0f }, { 0, 0.5f, 0, 1, 1.0f },
		{ 1, 128.0f / 255.0f, 1, 0, 1.0f }, { 0, 0.5f, 1, 1, 231.0f / 255.0f }, { 1, 216.0f / 255.0f, 1, 1, 1.0f },
		{ 0, 0.5f, 1, 0, 1.0f } };
	for ( int c = 0; c < 6; c++ ) {
		std::printf( "share %d %.4f %d %d %.4f", cs[c].now, double( cs[c].fi ), cs[c].ch, cs[c].last, double( cs[c].fo ) );
		for ( float p : pcts )
			std::printf( " %.6f", double( L::stormShare( cs[c].now, cs[c].fi, cs[c].ch, cs[c].last, cs[c].fo, p ) ) );
		std::printf( "\n" );
	}
	return 0;
}

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		return 2;
	if ( !std::strcmp( argv[1], "timing" ) )
		return timing( argc, argv );
	if ( !std::strcmp( argv[1], "field" ) )
		return field( argc, argv );
	if ( !std::strcmp( argv[1], "share" ) )
		return share();
	if ( !std::strcmp( argv[1], "switch" ) ) {
		for ( int i = 2; i < argc; i++ )
			std::printf( "switch %s %d\n", argv[i], L::switchOn( std::strcmp( argv[i], "-" ) ? argv[i] : nullptr ) ? 1 : 0 );
		return 0;
	}
	return 2;
}
