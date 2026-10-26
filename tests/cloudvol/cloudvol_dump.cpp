// lane CLOUD2: the volume clouds' CPU side for the offline gate (tests/spells/cloudvol_check.py builds this with g++).
// usage: cloudvol_dump <out dir> [band base top density sheet]...  writes shape.bin + detail.bin (the exe's noise bytes)
// and prints one "cal <i> v0..v16" line per band spec (the calibrated table) and "drop gHg gD alpha wD".
#include "gl/cloudnoise.h"
#include "gl/cloudvolmath.h"

#include <cstdio>
#include <cstdlib>
#include <string>

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		return 2;
	const std::string out = argv[1];
	const auto shape = WwCloudNoise::shapeVolume();
	const auto detail = WwCloudNoise::detailVolume();
	for ( auto [name, v] : { std::pair{ "/shape.bin", &shape }, std::pair{ "/detail.bin", &detail } } ) {
		FILE * f = std::fopen( ( out + name ).c_str(), "wb" );
		if ( !f )
			return 3;
		std::fwrite( v->data(), 1, v->size(), f );
		std::fclose( f );
	}
	for ( int a = 2, i = 0; a + 4 < argc; a += 5, i++ ) {
		float cal[WwCloudVolMath::kCalN];
		const bool red = std::getenv( "CLOUDVOL_NOCAL" ) != nullptr;
		WwCloudVolMath::calibrate( shape.data(), WwCloudNoise::kShapeN, detail.data(), WwCloudNoise::kDetailN,
			std::atoi( argv[a] ), float( std::atof( argv[a + 1] ) ), float( std::atof( argv[a + 2] ) ),
			float( std::atof( argv[a + 3] ) ), float( std::atof( argv[a + 4] ) ), red, cal );
		std::printf( "cal %d", i );
		for ( float c : cal )
			std::printf( " %.9g", double( c ) );
		std::printf( "\n" );
	}
	const auto d = WwCloudVolMath::dropletParams( WwCloudVolMath::kDropletUm );
	std::printf( "drop %.9g %.9g %.9g %.9g\n", double( d.gHg ), double( d.gD ), double( d.alpha ), double( d.wD ) );
	return 0;
}
