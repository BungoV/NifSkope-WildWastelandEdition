// lane FIX1031B clouds: prints the rain fog's extinction laws (src/gl/rainfogmath.h) for tests/rainfogscale, so the
// checker judges exactly the numbers cellvolair.cpp hands the shaders (never re-typed).
//   g++ -O2 -std=c++17 -I src tests/rainfogscale/rainfogscale_dump.cpp -o rainfogscale_dump.exe
#include "gl/rainfogmath.h"

#include <cstdio>

int main()
{
	namespace RF = WwRainFog;
	const WwPrecip::TypeInfo & ti = WwPrecip::typeInfo( WwPrecip::Rain );
	std::printf( "rain %.17g %.17g\n", ti.sizeMm, ti.rateMmH );
	std::printf( "mono %.17g\n", RF::defaultSigmaPerKm() );
	std::printf( "mp %.17g\n", RF::defaultMpSigmaPerKm() );
	std::printf( "mpconst %.17g %.17g %.17g %.17g\n", RF::kMpN0, RF::kMpLambda, RF::kMpExp, RF::kQext );
	for ( double r : { 0.5, 1.0, 2.0, 4.0, 8.5714285714285712, 16.0, 25.0 } )
		std::printf( "mprate %.17g %.17g\n", r, RF::mpSigmaPerKm( r ) );
	for ( double s : { 0.0, 0.5, 1.0, 15.0 / 7.0, 3.0 } )
		std::printf( "factor %.17g %.17g\n", s, RF::mpRateFactor( s ) );
	for ( const char * v : { "", "1", "old", "mono", "today", "mono,today", "x,mono", "monoo", "today2" } )
		std::printf( "words '%s' %d %d\n", v, RF::redWords( v ), RF::redOn( v ) ? 1 : 0 );
	return 0;
}
