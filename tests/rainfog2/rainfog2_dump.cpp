// lane RAINFOG2: prints the record rules (gl/precipmath.h spgdKind / spgdRateScale / spgdRedBits, gl/rainfogmath.h
// shares + the snow law, gl/precipspec.h wwPrecipFromSpgd) for tests/rainfog2/rainfog2_check.py, which judges them
// against its own transcription and the census fixture. -DRAINFOG2_RED prints today's rules instead (the class-flag
// particles, MNAM = rain at the default rate, no snow law): the red that must FAIL.
//   g++ -O2 -std=c++17 -I src -I<qt6> -I<qt6>/QtCore tests/rainfog2/rainfog2_dump.cpp -lQt6Core -o rainfog2_dump.exe
#include "gl/precipspec.h"
#include "gl/rainfogmath.h"

#include <cstdio>

namespace RF = WwRainFog;

static void layers( int type, double density, unsigned flags, int red )
{
#ifdef RAINFOG2_RED
	(void)type;
	(void)density;
	(void)red;
	const QVector<WwPrecipLayerSpec> v = wwPrecipFromRecord( flags );
#else
	const QVector<WwPrecipLayerSpec> v = wwPrecipFromSpgd( type, density, flags, red );
#endif
	std::printf( "layers %d %.17g %u %d %d", type, density, flags, red, int( v.size() ) );
	for ( const WwPrecipLayerSpec & s : v )
		std::printf( " %d:%.17g", s.type, s.rateMmH );
	std::printf( "\n" );
}

static void shares( const int type[2], const double dens[2], bool blending, double pct, bool snowOn )
{
	double rain = 0.0, snow = 0.0;
#ifdef RAINFOG2_RED
	(void)dens;
	(void)snowOn;
	rain = RF::share( type[0] >= -1, type[1] >= -1, blending, pct );	// CLOUD6e: MNAM = rain, the default rate
#else
	const double scale[2] = { WwPrecip::spgdRateScale( dens[0] ), WwPrecip::spgdRateScale( dens[1] ) };
	RF::shares( type, scale, blending, pct, snowOn, rain, snow );
#endif
	std::printf( "shares %d %d %.17g %.17g %d %.17g %d %.17g %.17g\n", type[0], type[1], dens[0], dens[1], blending ? 1 : 0, pct,
		snowOn ? 1 : 0, rain, snow );
}

int main()
{
	std::printf( "rainsigma %.17g\n", RF::defaultSigmaPerKm() );
#ifdef RAINFOG2_RED
	std::printf( "snowsigma %.17g\n", RF::defaultSigmaPerKm() );	// today: a snow shader takes the rain's law
#else
	std::printf( "snowsigma %.17g\n", RF::defaultSnowSigmaPerKm() );
	std::printf( "segments %d\n", RF::kTailSegments );
#endif
	const WwPrecip::TypeInfo & sn = WwPrecip::typeInfo( WwPrecip::SnowFine );
	const WwPrecip::TypeInfo & rn = WwPrecip::typeInfo( WwPrecip::Rain );
	std::printf( "snowdefault %.17g %.17g\n", sn.sizeMm, sn.rateMmH );
	std::printf( "raindefault %.17g %.17g\n", rn.sizeMm, rn.rateMmH );
	for ( int t = -3; t <= 2; t++ )
#ifdef RAINFOG2_RED
		std::printf( "kind %d %d\n", t, t >= -1 ? 1 : 0 );
#else
		std::printf( "kind %d %d\n", t, WwPrecip::spgdKind( t ) );
#endif
	for ( double d : { -1.0, 0.0, 3.5, 7.0, 15.0 } )
#ifdef RAINFOG2_RED
		std::printf( "scale %.17g %.17g\n", d, 1.0 );
#else
		std::printf( "scale %.17g %.17g\n", d, WwPrecip::spgdRateScale( d ) );
#endif
	const char * sws[] = { "", "1", "on", "0", "off", "false", "no" };
	const char * reds[] = { "", "flags", "rate", "flags,rate", "rate,flags", "1", "on", "old", "x", "0" };
	for ( const char * s : sws )
		for ( const char * r : reds )
#ifdef RAINFOG2_RED
			std::printf( "redbits '%s' '%s' %d\n", s, r, 3 );
#else
			std::printf( "redbits '%s' '%s' %d\n", s, r, WwPrecip::spgdRedBits( s, r ) );
#endif
	for ( unsigned f : { 0u, 4u, 8u, 12u, 24u, 196u } )
		std::printf( "flagkinds %u %d\n", f, WwPrecip::flagKinds( f ) );
	const int types[] = { -2, -1, 0, 1 };
	const double dens[] = { -1.0, 7.0, 15.0 };
	for ( int t : types )
		for ( double d : dens )
			for ( unsigned f : { 0u, 4u, 8u, 12u } )
				for ( int red = 0; red < 4; red++ )
					layers( t, d, f, red );
	for ( int a : types )
		for ( int b : types )
			for ( double da : dens )
				for ( double db : dens )
					for ( int bl = 0; bl < 2; bl++ )
						for ( int k = 0; k <= 4; k++ )
							for ( int so = 0; so < 2; so++ ) {
								const int ty[2] = { a, b };
								const double de[2] = { da, db };
								shares( ty, de, bl != 0, k * 0.25, so != 0 );
							}
	return 0;
}
