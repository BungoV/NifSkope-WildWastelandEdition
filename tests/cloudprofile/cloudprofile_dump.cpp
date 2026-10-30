// lane CLOUD6d: the Low band's cloud type per weather class through the REAL rule (src/gl/cloudmath.h lowTypeOf) with the
// profile set on / off, and the switch's on/off rule (src/gl/cloudvolmath.h defaultOnSwitch, as cloudvol.cpp cs() reads
// WW_CLOUDVOL_PROFILESET). No GL, no Qt; tests/spells/cloudprofile_check.py builds this with g++.
//
//   cloudprofile_dump type <flags> <thunder> <oneSet 0|1>   -> "type <t> spread <s>"
//   cloudprofile_dump switch [value]                         -> "switch on|off" (no value = unset)
#include "gl/cloudmath.h"
#include "gl/cloudvolmath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main( int argc, char ** argv )
{
	if ( argc >= 5 && !std::strcmp( argv[1], "type" ) ) {
		float t = 0.0f, s = 0.0f;
		WwCloudMath::lowTypeOf( unsigned( std::atoi( argv[2] ) ), unsigned( std::atoi( argv[3] ) ), -1.0f, t, s,
			std::atoi( argv[4] ) != 0 );
		std::printf( "type %.6f spread %.6f\n", double( t ), double( s ) );
		return 0;
	}
	if ( argc >= 2 && !std::strcmp( argv[1], "switch" ) ) {
		std::printf( "switch %s\n", WwCloudVolMath::defaultOnSwitch( argc >= 3 ? argv[2] : nullptr ) ? "on" : "off" );
		return 0;
	}
	std::fprintf( stderr, "usage: cloudprofile_dump type <flags> <thunder> <oneSet> | switch [value]\n" );
	return 2;
}
