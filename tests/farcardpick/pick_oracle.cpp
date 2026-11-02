/* The CPU column of the far card pick unit (lane FARCARDPICK1, 2026-11-02).
 *
 * The shipped CPU pick, from the shipped sources: src/gl/impostorpick.h (selectFrames' ring and shuffle, pickCard's
 * sharpen / snap / billboard) and src/impostoroct.cpp (pickFramesForCamera and both conventions), in the order
 * pickCard runs them. Compiled with the product's flags (-O3 -march=haswell). Reads rows
 *   eye.xyz centre.xyz N V ring asBaked shuffle sharpen frameCount   (anything after a '|' is ignored)
 * and writes one line a row:
 *   idx0 idx1 idx2 w0 w1 w2 right.xyz up.xyz fwd.xyz | boundary
 * where boundary is the double-precision distance of the row's pick to the nearest place a frame index can change:
 * a ring's floor / half step (in frames), a grid's floor / diagonal (in grid cells), a snap's tie between the two
 * strongest weights. tests/farcardpick/farcardpick.py compares the GPU against it.
 *
 *   g++ -std=c++20 -O3 -march=haswell -I<tree>/src -I<tree>/src/gl pick_oracle.cpp <tree>/src/impostoroct.cpp
 */
#include "impostoroct.h"
#include "impostorpick.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static double frac( double x ) { return x - std::floor( x ); }
static double edge( double x ) { const double f = frac( x ); return std::min( f, 1.0 - f ); }

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc < 3 ) {
		std::fprintf( stderr, "usage: pick_oracle <rows> <out>\n" );
		return 2;
	}
	std::FILE * in = std::fopen( argv[1], "r" );
	std::FILE * out = std::fopen( argv[2], "w" );
	if ( !in || !out ) {
		std::fprintf( stderr, "cannot open\n" );
		return 2;
	}
	char line[1024];
	long rows = 0;
	while ( std::fgets( line, sizeof( line ), in ) ) {
		if ( line[0] == '#' || line[0] == '\n' )
			continue;
		float eye[3], centre[3], sharpen;
		int N, V, ring, asBaked, shuffle, fc;
		if ( std::sscanf( line, "%f %f %f %f %f %f %d %d %d %d %d %f %d", &eye[0], &eye[1], &eye[2], &centre[0],
				&centre[1], &centre[2], &N, &V, &ring, &asBaked, &shuffle, &sharpen, &fc ) != 13 ) {
			std::fprintf( stderr, "bad row: %s", line );
			return 2;
		}
		// selectFrames
		float d[3];
		ImpostorPick::viewDir( eye, centre, d );
		int idx[3] = { 0, 0, 0 };
		float w[3] = { 0.0f, 0.0f, 0.0f };
		double boundary = 1e30;
		if ( ring ) {
			ImpostorPick::ringFrames( d, V, shuffle != 0, idx, w );
			const double twoPi = 6.283185307179586;
			double phi = std::atan2( double( d[1] ), double( d[0] ) );
			if ( phi < 0.0 )
				phi += twoPi;
			const double f = phi / twoPi * double( V );
			boundary = std::min( edge( f ), std::fabs( frac( f ) - 0.5 ) );
		} else {
			int gi[3], gj[3];
			ImpostorOct::g_convention = asBaked ? ImpostorOct::Convention::AsBaked : ImpostorOct::Convention::SpecLiteral;
			ImpostorOct::pickFramesForCamera( d, N, idx, gi, gj, w );
			ImpostorOct::g_convention = ImpostorOct::Convention::SpecLiteral;
			if ( shuffle )
				ImpostorPick::gridShuffle( N, idx );
			// the grid coordinates in double, from the same float direction
			double e[3] = { asBaked ? -d[0] : d[0], asBaked ? -d[1] : d[1], d[2] };
			double n = std::sqrt( e[0] * e[0] + e[1] * e[1] + e[2] * e[2] );
			if ( n > 1e-20 ) {
				for ( double & c : e )
					c /= n;
				if ( e[2] < 0.0 ) {
					e[2] = 0.0;
					n = std::sqrt( e[0] * e[0] + e[1] * e[1] );
					if ( n > 1e-20 ) { e[0] /= n; e[1] /= n; }
				}
				const double L = std::fabs( e[0] ) + std::fabs( e[1] ) + e[2];
				if ( L > 1e-20 ) {
					const double x = e[0] / L, y = e[1] / L;
					const double a = std::clamp( ( x + y + 1.0 ) * 0.5 * ( N - 1 ), 0.0, double( N - 1 ) );
					const double b = std::clamp( ( x - y + 1.0 ) * 0.5 * ( N - 1 ), 0.0, double( N - 1 ) );
					const double fa = a - std::clamp( std::floor( a ), 0.0, double( N - 2 ) );
					const double fb = b - std::clamp( std::floor( b ), 0.0, double( N - 2 ) );
					// an interior floor only (a clamped edge cannot move the index), and the diagonal
					if ( a > 0.0 && a < N - 1 ) boundary = std::min( boundary, edge( a ) );
					if ( b > 0.0 && b < N - 1 ) boundary = std::min( boundary, edge( b ) );
					boundary = std::min( boundary, std::fabs( fa + fb - 1.0 ) );
				}
			}
		}
		// pickCard
		float wRaw[3] = { w[0], w[1], w[2] };
		ImpostorPick::sharpenWeights( fc, sharpen, idx, w );
		if ( fc == 1 ) {	// the snap's tie: the two strongest raw weights
			float s[3] = { wRaw[0], wRaw[1], wRaw[2] };
			std::sort( s, s + 3 );
			boundary = std::min( boundary, double( s[2] ) - double( s[1] ) );
		}
		float right[3], up[3];
		ImpostorPick::billboard( d, right, up );
		std::fprintf( out, "%d %d %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g | %.6g\n", idx[0], idx[1],
			idx[2], double( w[0] ), double( w[1] ), double( w[2] ), double( right[0] ), double( right[1] ),
			double( right[2] ), double( up[0] ), double( up[1] ), double( up[2] ), double( d[0] ), double( d[1] ),
			double( d[2] ), boundary );
		rows++;
	}
	std::fclose( out );
	std::printf( "oracle: %ld rows\n", rows );
	return 0;
}
