// lane CLOUDTYPE2: the Low band's cloud type of every weather in a census fixture through the REAL rule
// (src/gl/cloudmath.h lowTypeOf) with the thunder sounds (the default) and with CLOUD5's thunder byte (thunderSounds -1,
// the red), plus the switch rule (src/gl/cloudvolmath.h defaultOnSwitch, as cloudvol.cpp cs() reads
// WW_CLOUDVOL_TYPERULE). No GL, no Qt; tests/spells/cloudtype_check.py builds this with g++.
//
//   cloudtype_dump rows <weathers.tsv>   -> "row <edid> <flags> <byte> <sounds> <new type> <new spread> <old type> <old spread>"
//   cloudtype_dump switch [value]        -> "switch on|off" (no value = unset)
#include "gl/cloudmath.h"
#include "gl/cloudvolmath.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main( int argc, char ** argv )
{
	if ( argc >= 3 && !std::strcmp( argv[1], "rows" ) ) {
		FILE * f = std::fopen( argv[2], "rb" );
		if ( !f )
			return 3;
		char line[512];
		while ( std::fgets( line, sizeof( line ), f ) ) {
			if ( line[0] == '#' )
				continue;
			char edid[256];
			unsigned flags = 0, byte = 0;
			int sounds = 0, precip = 0;
			if ( std::sscanf( line, "%255s %u %u %d %d", edid, &flags, &byte, &sounds, &precip ) < 4 )
				continue;
			float tn = 0, sn = 0, to = 0, so = 0;
			WwCloudMath::lowTypeOf( flags, byte, -1.0f, tn, sn, false, sounds );
			WwCloudMath::lowTypeOf( flags, byte, -1.0f, to, so, false, -1 );
			std::printf( "row %s %u %u %d %.6f %.6f %.6f %.6f\n", edid, flags, byte, sounds, double( tn ), double( sn ),
				double( to ), double( so ) );
		}
		std::fclose( f );
		return 0;
	}
	if ( argc >= 2 && !std::strcmp( argv[1], "switch" ) ) {
		std::printf( "switch %s\n", WwCloudVolMath::defaultOnSwitch( argc >= 3 ? argv[2] : nullptr ) ? "on" : "off" );
		return 0;
	}
	std::fprintf( stderr, "usage: cloudtype_dump rows <weathers.tsv> | switch [value]\n" );
	return 2;
}
