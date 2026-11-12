// lane VKLIGHT1 (2026-11-12): the contact march's band list from the app's own header (src/features/shadows/contactlines.h),
// for tests/vklight/light_gl_ref.py: both GL's and Vulkan's run read these bytes.
//   contact_bands <lx> <ly> <mw> <mh> <out bands.bin>
// lx / ly are parsed as float (the app passes float light[] to wwContactBands). Prints `bands N waves W first a b c d`.
#include "features/shadows/contactlines.h"
#include <cstdio>
#include <cstdlib>

int main( int argc, char ** argv )
{
	if ( argc < 6 ) {
		std::printf( "usage: contact_bands <lx> <ly> <mw> <mh> <out>\n" );
		return 2;
	}
	const float lx = std::strtof( argv[1], nullptr ), ly = std::strtof( argv[2], nullptr );
	const WwContactBands B = wwContactBands( lx, ly, std::atoi( argv[3] ), std::atoi( argv[4] ) );
	FILE * f = std::fopen( argv[5], "wb" );
	if ( !f )
		return 1;
	if ( !B.bands.empty() )
		std::fwrite( B.bands.data(), sizeof( WwContactBand ), B.bands.size(), f );
	std::fclose( f );
	std::printf( "bands %d waves %d first %d %d %d %d\n", int( B.bands.size() ), B.waves, B.first[0], B.first[1], B.first[2], B.first[3] );
	return 0;
}
