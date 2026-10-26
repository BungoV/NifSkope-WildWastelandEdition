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
	// lane CLOUD2B: the multiple-scattering floor's inputs (the checker compares msDiffuseT with its own transcription)
	const float g = WwCloudVolMath::msMeanCosine( d );
	std::printf( "msg %.9g\n", double( g ) );
	for ( float tau : { 0.5f, 2.0f, 8.0f, 30.0f, 400.0f } )
		for ( float mu : { 0.1f, 0.5f, 0.866f } )
			std::printf( "tdif %g %g %.9g\n", double( tau ), double( mu ), double( WwCloudVolMath::msDiffuseT( tau, mu, g, WwCloudVolMath::kAlbedo ) ) );
	// lane CLOUD2C: the top reflection's twin; the wind offset the draw uses and the one the readers' bind hands out
	for ( float tau : { 0.5f, 2.0f, 8.0f, 30.0f, 400.0f } )
		for ( float mu : { 0.1f, 0.5f, 0.866f } )
			std::printf( "rdir %g %g %.9g\n", double( tau ), double( mu ), double( WwCloudVolMath::msDiffuseR( tau, mu, g, WwCloudVolMath::kAlbedo ) ) );
	const bool wind0 = std::getenv( "CLOUDVOL_WIND0" ) != nullptr;
	for ( double t : { 0.0, 60.0, 600.0, 3600.0, 86400.0 } )
		std::printf( "wind %g %.9g %.9g\n", t, double( WwCloudVolMath::windKm( t ) ), double( WwCloudVolMath::readerWindKm( t, wind0 ) ) );
	return 0;
}
