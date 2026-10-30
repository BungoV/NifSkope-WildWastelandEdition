// lane CLOUD6e: prints the rain fog's CPU numbers (src/gl/rainfogmath.h) for tests/spells/rainfog_check.py, so the
// checker uploads and judges exactly what cellvolair.cpp hands the shader (never re-typed).
//   g++ -O2 -std=c++17 -I src tests/rainfog/rainfog_dump.cpp -o rainfog_dump.exe
#include "gl/rainfogmath.h"

#include <cstdio>

int main()
{
	namespace RF = WwRainFog;
	const WwPrecip::TypeInfo & ti = WwPrecip::typeInfo( WwPrecip::Rain );
	std::printf( "rain %.17g %.17g\n", ti.sizeMm, ti.rateMmH );
	std::printf( "sigma %.17g\n", RF::defaultSigmaPerKm() );
	// a precipitation red must not reach it
	WwPrecip::redBits() = WwPrecip::RedConstDensity | WwPrecip::RedOneSpeed;
	std::printf( "sigmared %.17g\n", RF::defaultSigmaPerKm() );
	WwPrecip::redBits() = 0;
	std::printf( "law %.17g %.17g %.17g %.17g\n", RF::kBaseExcess, RF::kFalloffKm, RF::kAboveKm, RF::kEdgeFade );
	for ( int i = -40; i <= 600; i++ ) {
		const double h = i * 0.005;
		std::printf( "profile %.17g %.17g\n", h, RF::profile( h ) );
	}
	for ( int c = 0; c < 2; c++ )
		for ( int o = 0; o < 2; o++ )
			for ( int b = 0; b < 2; b++ )
				for ( int k = 0; k <= 10; k++ )
					std::printf( "share %d %d %d %.17g %.17g\n", c, o, b, k * 0.1, RF::share( c, o, b, k * 0.1 ) );
	for ( const char * v : { "", "1", "on", "yes", "true", "2", "0", "off", "false", "no" } )
		std::printf( "switch '%s' %d\n", v, RF::switchOn( v ) ? 1 : 0 );
	for ( const char * v : { "", "0", "off", "1", "on", "old" } )
		std::printf( "red '%s' %d\n", v, RF::redOn( v ) ? 1 : 0 );
	return 0;
}
