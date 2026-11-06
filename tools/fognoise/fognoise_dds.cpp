/* MODULES1 part 19: writes the shipped fog noise (res/Data/Textures/FO4CS/Fog/GroundFogNoise.dds) from the generator
 * the viewer used before the file (src/features/air/fognoise.h), and checks a file against it.
 * Build with the viewer's own flags, so the floats round as they did in it:
 *   g++ -std=gnu++2a -O3 -march=haswell -mtune=generic -Isrc tools/fognoise/fognoise_dds.cpp -o fognoise_dds.exe
 * Use: fognoise_dds write <out.dds> | fognoise_dds check <in.dds>   (check: "SAME <n> texels mean .. span .." rc 0) */

#include "features/air/fognoise.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

int main( int argc, char ** argv )
{
	if ( argc != 3 || ( std::strcmp( argv[1], "write" ) != 0 && std::strcmp( argv[1], "check" ) != 0 ) ) {
		std::fprintf( stderr, "use: fognoise_dds write|check <file.dds>\n" );
		return 2;
	}
	std::vector<std::uint8_t> tex;
	WwFogNoise::build( tex );
	float mean = 0, span = 0;
	WwFogNoise::stats( tex, mean, span );
	if ( std::strcmp( argv[1], "write" ) == 0 ) {
		const std::vector<std::uint8_t> o = WwFogNoise::ddsWrite( tex, WwFogNoise::kN );
		std::ofstream f( argv[2], std::ios::binary );
		f.write( reinterpret_cast<const char *>( o.data() ), std::streamsize( o.size() ) );
		if ( !f )
			return 1;
		std::printf( "wrote %s: %zu bytes, %d^3 R8, mean %.6f span %.6f\n", argv[2], o.size(), WwFogNoise::kN, mean, span );
		return 0;
	}
	std::ifstream f( argv[2], std::ios::binary );
	const std::vector<std::uint8_t> b( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
	int n = 0;
	std::vector<std::uint8_t> got;
	if ( !WwFogNoise::ddsRead( b.data(), b.size(), n, got ) ) {
		std::printf( "REFUSED %s: not a cubic R8 volume .dds\n", argv[2] );
		return 1;
	}
	if ( got != tex ) {
		std::size_t diff = 0;
		for ( std::size_t i = 0; i < std::min( got.size(), tex.size() ); i++ )
			diff += got[i] != tex[i];
		std::printf( "DIFFERS %s: edge %d, %zu of %zu texels differ\n", argv[2], n, diff, tex.size() );
		return 1;
	}
	std::printf( "SAME %zu texels mean %.6f span %.6f\n", tex.size(), mean, span );
	return 0;
}
