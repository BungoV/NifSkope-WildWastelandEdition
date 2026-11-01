/* lane PERFFIX2 b (item 5b): gl/cubefacecull.h, the per-face caster cull of the light cubes.
 * Green: no sphere is dropped from a face it can put a fragment into (dense points inside each sphere, through the
 * face's own clip rows), every face mask is a subset of {0..5}, and the cull saves draws (the mean faces per caster).
 * Red -DCUBEFACECULL_RED (the centre-only test): must FAIL "no fragment lost".
 * Also checks that celllights.cpp still carries the same face table (its projection and the header must agree).
 * Usage: cubefacecull_test <tree root> */
#include "gl/cubefacecull.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

static int fails = 0;

static void check( bool ok, const char * what, const std::string & detail = std::string() )
{
	std::printf( "%s %s%s%s\n", ok ? "ok" : "FAIL", what, detail.empty() ? "" : ": ", detail.c_str() );
	if ( !ok )
		fails++;
}

// the face's clip rows, as celllights.cpp builds shClipFromRel (x = u.r, y = v.r, w = m.r), in double
static bool inFace( int f, const double p[3] )
{
	double x = 0, y = 0, w = 0;
	for ( int k = 0; k < 3; k++ ) {
		x += WwCubeFace::kU[f][k] * p[k];
		y += WwCubeFace::kV[f][k] * p[k];
		w += WwCubeFace::kM[f][k] * p[k];
	}
	return w > 0.0 && std::fabs( x ) <= w && std::fabs( y ) <= w;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	std::mt19937 rng( 0x5eedu );
	std::uniform_real_distribution<double> uni( -1.0, 1.0 ), u01( 0.0, 1.0 );

	// 1. no fragment lost: a sphere is kept in every face that a point of it lands in
	long lost = 0, spheres = 0, facesKept = 0, facesUsed = 0;
	std::string firstLost;
	for ( int n = 0; n < 40000; n++ ) {
		const double lightR = 64.0 + 2000.0 * u01( rng );
		double c[3];
		for ( double & v : c )
			v = uni( rng ) * lightR;
		// casters from a few units to the light's size, many straddling a face edge
		const double rad = std::pow( 10.0, -0.5 + 3.0 * u01( rng ) ) * ( n % 3 ? 0.05 : 0.5 ) * lightR / 50.0 + 0.5;
		const float cf[3] = { float( c[0] ), float( c[1] ), float( c[2] ) };
		const int mask = WwCubeFace::mask( cf, float( rad ) );
		if ( mask & ~63 )
			lost += 1000000;
		spheres++;
		int used = 0;
		for ( int s = 0; s < 400; s++ ) {
			double d[3];
			double l2 = 0;
			do {
				for ( double & v : d )
					v = uni( rng );
				l2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
			} while ( l2 > 1.0 || l2 < 1e-9 );
			const double k = s % 2 ? rad / std::sqrt( l2 ) : rad;	// half on the surface, half inside
			const double p[3] = { c[0] + d[0] * k, c[1] + d[1] * k, c[2] + d[2] * k };
			for ( int f = 0; f < 6; f++ ) {
				if ( inFace( f, p ) ) {
					used |= 1 << f;
					if ( !( mask >> f & 1 ) ) {
						if ( firstLost.empty() ) {
							char b[200];
							std::snprintf( b, sizeof b, "face %d centre %.1f,%.1f,%.1f radius %.2f", f, c[0], c[1], c[2], rad );
							firstLost = b;
						}
						lost++;
					}
				}
			}
		}
		for ( int f = 0; f < 6; f++ ) {
			facesKept += mask >> f & 1;
			facesUsed += used >> f & 1;
		}
	}
	check( lost == 0, "no fragment lost (40000 spheres x 400 points, every face)",
		lost ? std::to_string( lost ) + " points; first " + firstLost : std::string() );
	char b[160];
	std::snprintf( b, sizeof b, "%.2f faces per caster kept (of 6), %.2f really reached", double( facesKept ) / double( spheres ),
		double( facesUsed ) / double( spheres ) );
	check( double( facesKept ) / double( spheres ) < 4.0, "the cull saves draws", b );

	// 2. a caster round the light reaches every face; a small one on an axis reaches one
	{
		const float c0[3] = { 0, 0, 0 };
		check( WwCubeFace::mask( c0, 10.0f ) == 63, "a caster round the light: all six faces" );
		const float c1[3] = { 500, 0, 0 };
		check( WwCubeFace::mask( c1, 20.0f ) == 1, "a small caster on +x: face 0 only" );
		const float c2[3] = { 0, 0, -800 };
		check( WwCubeFace::mask( c2, 30.0f ) == 32, "a small caster on -z: face 5 only" );
	}

	// 3. celllights.cpp's face table is the header's
	if ( argc > 1 ) {
		std::ifstream in( std::string( argv[1] ) + "/src/gl/celllights.cpp" );
		std::stringstream ss;
		ss << in.rdbuf();
		const std::string src = ss.str();
		const char * m = "{ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }";
		const char * u = "{ { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } }";
		const char * v = "{ { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } }";
		check( !src.empty() && src.find( m ) != std::string::npos && src.find( u ) != std::string::npos
			&& src.find( v ) != std::string::npos, "celllights.cpp carries the same face table" );
		check( src.find( "WwCubeFace::touches" ) != std::string::npos, "celllights.cpp culls with WwCubeFace::touches" );
	}
	std::printf( "CUBEFACECULL %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
