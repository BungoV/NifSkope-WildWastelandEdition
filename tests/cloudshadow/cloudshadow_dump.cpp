// lane CLOUD3: prints src/gl/cloudshadowmath.h's light space for the offline gate (tests/spells/cloudshadow_check.py),
// so the checker uploads the exe's own basis, fit and matrix instead of re-typing them.
// usage: cloudshadow_dump sunX sunY sunZ redVertical eyeKmX eyeKmY eyeKmZ extentKm size unitKm

#include "gl/cloudshadowmath.h"

#include <cstdio>
#include <cstdlib>

namespace M = WwCloudShadowMath;

int main( int argc, char ** argv )
{
	if ( argc < 11 ) {
		std::fprintf( stderr, "usage: sunX sunY sunZ redVertical eyeKmX eyeKmY eyeKmZ extentKm size unitKm\n" );
		return 2;
	}
	const float sun[3] = { float( std::atof( argv[1] ) ), float( std::atof( argv[2] ) ), float( std::atof( argv[3] ) ) };
	const bool red = std::atoi( argv[4] ) != 0;
	const double eye[3] = { std::atof( argv[5] ), std::atof( argv[6] ), std::atof( argv[7] ) };
	const M::Basis b = M::basis( sun, red );
	const M::Fit f = M::fit( eye, std::atof( argv[8] ), std::atoi( argv[9] ), b );
	double L[16];
	M::lightFromWorld( b, f, std::atof( argv[10] ), L );
	std::printf( "basis %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", b.s[0], b.s[1], b.s[2], b.e1[0], b.e1[1], b.e1[2],
		b.e2[0], b.e2[1], b.e2[2] );
	std::printf( "fit %.9g %.9g %.9g %.9g\n", f.a0, f.b0, f.extentKm, f.texelKm() );
	std::printf( "mat" );
	for ( double v : L )
		std::printf( " %.9g", v );
	std::printf( "\n" );
	return 0;
}
