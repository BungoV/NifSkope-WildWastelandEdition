// lane CLOUD4: prints src/gl/cloudflymath.h's numbers for the offline gate (tests/spells/cloudfly_check.py), so the checker
// uses the exe's own presets, placement, age and Chebyshev reference instead of re-typing them.
// usage: cloudfly_dump presets
//        cloudfly_dump place <tier> camX camY camZ lo hi          -> org x y z
//        cloudfly_dump age <tier> ageSecs windKmS camMoveKm       -> cells
//        cloudfly_dump sdf <occ.bin> nx ny nz <out.bin>            (R8 0/1 occupancy in, distance cells out)

#include "gl/cloudflymath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace M = WwCloudFlyMath;

int main( int argc, char ** argv )
{
	if ( argc >= 2 && !std::strcmp( argv[1], "presets" ) ) {
		for ( int t = 1; t <= 4; t++ ) {
			const M::Preset & p = M::preset( t );
			std::printf( "preset %d %d %d %.9g %.9g %d %.9g\n", t, p.nxy, p.nz, p.cellKm, p.stepKm, p.maxSteps, M::memoryBytes( p ) );
		}
		std::printf( "const %d %d %.9g %.9g %.9g %d %d\n", M::kCap, M::kSafe, M::kNearKm, M::kMarginS, M::kSlackKm, M::kFrames,
			M::kStaleCells );
		return 0;
	}
	if ( argc >= 8 && !std::strcmp( argv[1], "place" ) ) {
		const float cam[3] = { float( std::atof( argv[3] ) ), float( std::atof( argv[4] ) ), float( std::atof( argv[5] ) ) };
		float org[3];
		M::place( M::preset( std::atoi( argv[2] ) ), cam, float( std::atof( argv[6] ) ), float( std::atof( argv[7] ) ), org );
		std::printf( "org %.9g %.9g %.9g\n", org[0], org[1], org[2] );
		return 0;
	}
	if ( argc >= 6 && !std::strcmp( argv[1], "age" ) ) {
		std::printf( "cells %d\n", M::ageCells( M::preset( std::atoi( argv[2] ) ), std::atof( argv[3] ), float( std::atof( argv[4] ) ),
			float( std::atof( argv[5] ) ) ) );
		return 0;
	}
	if ( argc >= 7 && !std::strcmp( argv[1], "sdf" ) ) {
		const int nx = std::atoi( argv[3] ), ny = std::atoi( argv[4] ), nz = std::atoi( argv[5] );
		std::vector<unsigned char> occ( size_t( nx ) * ny * nz ), out( occ.size() );
		FILE * f = std::fopen( argv[2], "rb" );
		if ( !f || std::fread( occ.data(), 1, occ.size(), f ) != occ.size() )
			return 3;
		std::fclose( f );
		M::chebyshev( occ.data(), nx, ny, nz, out.data() );
		f = std::fopen( argv[6], "wb" );
		if ( !f )
			return 4;
		std::fwrite( out.data(), 1, out.size(), f );
		std::fclose( f );
		std::printf( "sdf %zu\n", out.size() );
		return 0;
	}
	std::fprintf( stderr, "usage: presets | place | age | sdf\n" );
	return 2;
}
