#ifndef WW_RAINFOGMATH_H
#define WW_RAINFOGMATH_H

/* lane CLOUD6e (CLOUDBAND1 leftover, RDR2 gap e): the rain fog under the rain clouds, the law the air pass draws
 * (res/shaders/cell_rainfog.glsl, src/gl/cellvolair.cpp) and its unit test (tests/rainfog). Plain C++, no Qt, no GL.
 *
 * RDR2 (Bauer 2019, SIGGRAPH Advances, slide 19 + its note): "rain fog beneath clouds, where placement is derived off
 * the density in the cloud map. A height-based falloff is applied to connect the fog to the cloud." The talk gives no
 * numbers. Ours:
 *   which weathers  those whose record links a precipitation shader (WTHR MNAM, an SPGD): the census of the 107 vanilla
 *                   weathers (notes/fix1030c/cloud6e/census_rain.tsv) finds 10, every one flagged Rainy; 4 more carry
 *                   the Rainy flag with no precipitation shader (DiamondWeather x2, TCommonwealthMarshOvercast,
 *                   CommonwealthGSRadstormOld): they do not rain in the game, so no rain fog. A weather change weighs
 *                   the new weather by pct and the leaving one by 1 - pct (gl/precipmath.h blendWeight).
 *   where           under the Low band's map coverage c (CLOUD1's cloud map, R), straight down (rain falls; the map is
 *                   the coverage of the cloud above the point): sigma follows c linearly (the area share that rains)
 *   how dense       near the ground the extinction of the rain itself, sigma = N Q pi D^2 / 4: N the drops per m^3 that
 *                   carry the weather's rate (gl/precipmath.h densityFromRate, the particles' own law), D their size,
 *                   Q = 2 (large drops: the extinction paradox, van de Hulst 1957). The default rain (2 mm, 4 mm/h)
 *                   gives 0.254 / km (a visibility of 15 km, 3.912 / sigma, Koschmieder)
 *   height          rho(h) = 1 + kBaseExcess exp(-h / kFalloffKm) at h below the base (RDR2's falloff, joining the
 *                   cloud: the mist under the base), then a linear fade to 0 over kAboveKm above the base (the cloud
 *                   takes over; no line where the cloud is thin). kBaseExcess, kFalloffKm, kAboveKm are look numbers.
 * The air pass adds sigma x the sub-step's length to each sub-step's optical depth (the midpoint); every term the air
 * lights (sun, cloud shadow, lights, GI, ambient) then lights the rain fog too.
 * Lane RAINFOG2: the closed-form tails past the grids and the sky's tail carry it too (lookdev_fog.glsl wwAirRainTau:
 * kTailSegments sub-segments of the cloud map x the profile's exact mean; WW_CLOUD_RAINFOG_TAIL, red _RED); the rate
 * follows the weather's precipitation shader (SPGD density / 7, WW_PRECIP_SPGD); snow shaders get the snow law
 * (WW_CLOUD_SNOWFOG). */

#include "gl/precipmath.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace WwRainFog
{

constexpr double kQext = 2.0;			// extinction efficiency of drops much larger than the wavelength
constexpr double kBaseExcess = 1.0;		// the density at the base over the density near the ground, minus 1
constexpr double kFalloffKm = 0.3;		// the falloff from the base down (RDR2 slide 19's "height based falloff")
constexpr double kAboveKm = 0.15;		// the fade above the base, into the cloud
constexpr double kEdgeFade = 0.1;		// the map's edge band (uv) where c fades to the band's coverage scalar
constexpr int kTailSegments = 8;		// lane RAINFOG2: the tails' sub-segments (the map read at each one's middle)

//! the rain's extinction per km of a rain layer of size D mm at rateMmH (the particles' own drop count; the
//! precipitation reds do not reach it)
inline double sigmaPerKm( double D, double rateMmH )
{
	int & rb = WwPrecip::redBits();
	const int saved = rb;
	rb = 0;
	const double n = WwPrecip::densityFromRate( WwPrecip::Rain, D, rateMmH );
	rb = saved;
	const double r = D * 0.5e-3;	// m
	return n * kQext * 3.14159265358979 * r * r * 1000.0;
}

//! the default rain layer's (the precipitation the record-only weathers draw: precipmath.h typeInfo(Rain))
inline double defaultSigmaPerKm()
{
	const WwPrecip::TypeInfo & ti = WwPrecip::typeInfo( WwPrecip::Rain );
	return sigmaPerKm( ti.sizeMm, ti.rateMmH );
}

/* lane FIX1031B clouds (bungo's run3b CommonwealthRain pair: the rain fog moved the frame by 0.6 level). The fog is the
 * WHOLE rain's extinction, not one drop size's: Marshall & Palmer (1948, J. Meteor. 5, 165) drop sizes
 * N(D) = N0 exp(-Lambda D), N0 = 8000 m^-3 mm^-1, Lambda = 4.1 R^-0.21 mm^-1 (R mm/h), Q 2 per drop:
 * sigma = Q pi / 4 x N0 x 2 / Lambda^3 (mm^2 per m^3). The default rain (4 mm/h): 0.874 / km (visibility 4.5 km),
 * 3.43x the 2-mm drops at the same rate (the shading area sits in the small drops the particle layer never draws).
 * sigma goes as R^0.63: a rate scale s (the SPGD's density / 7) multiplies it by s^0.63 (mpRateFactor). The old law
 * (sigmaPerKm, one drop size) stays as the red WW_CLOUD_RAINFOG_RED=mono. */
constexpr double kMpN0 = 8000.0;		// m^-3 mm^-1
constexpr double kMpLambda = 4.1;		// mm^-1 at 1 mm/h
constexpr double kMpExp = 0.21;		// Lambda = kMpLambda R^-kMpExp

inline double mpSigmaPerKm( double rateMmH )
{
	if ( !( rateMmH > 0.0 ) )
		return 0.0;
	const double lam = kMpLambda * std::pow( rateMmH, -kMpExp );
	return kQext * 0.25 * 3.14159265358979 * kMpN0 * 2.0 / ( lam * lam * lam ) * 1e-6 * 1000.0;
}

//! the default rain layer's rain fog extinction (the shipped law)
inline double defaultMpSigmaPerKm()
{
	return mpSigmaPerKm( WwPrecip::typeInfo( WwPrecip::Rain ).rateMmH );
}

//! the factor a rate scale s puts on the Marshall-Palmer extinction (s^(3 x 0.21)); 1 at s = 1 exactly
inline double mpRateFactor( double s )
{
	return s == 1.0 ? 1.0 : ( s > 0.0 ? std::pow( s, 3.0 * kMpExp ) : 0.0 );
}

/* the words of WW_CLOUD_RAINFOG_RED besides "1 / on / old" (redOn: no rain fog at all), comma separated:
 * mono = the one-drop-size law (CLOUD6e's scale), today = mono and no tails (main 5422be6c's rain fog exactly) */
enum RedWord : int { RedMono = 1, RedNoTail = 2 };
inline int redWords( const char * v )
{
	int bits = 0;
	const char * p = v;
	while ( p && *p ) {
		const char * e = std::strchr( p, ',' );
		const std::size_t n = e ? std::size_t( e - p ) : std::strlen( p );
		auto is = [&]( const char * w ) { return std::strlen( w ) == n && std::strncmp( p, w, n ) == 0; };
		if ( is( "mono" ) )
			bits |= RedMono;
		else if ( is( "today" ) )
			bits |= RedMono | RedNoTail;
		p = e ? e + 1 : nullptr;
	}
	return bits;
}

/* lane RAINFOG2 (SNOWFOG1): the same extinction for a snow layer, sigma = N Q pi D^2 / 4 with N the particles' own
 * count at the layer's rate (gl/precipmath.h: dry snow's mass and fall speed), D the flake's size taken as the disc it
 * shades (an upper bound: an aggregate shades less than its outline), Q 2. The default snow layer (snow_fine, 5.5 mm,
 * 1 mm/h of water) gives 15.8 / km (visibility 0.25 km). No vanilla weather snows (no SPGD of Type Snow: census). */
inline double sigmaPerKmOf( int type, double D, double rateMmH )
{
	int & rb = WwPrecip::redBits();
	const int saved = rb;
	rb = 0;
	const double n = WwPrecip::densityFromRate( type, D, rateMmH );
	rb = saved;
	const double r = D * 0.5e-3;	// m
	return n * kQext * 3.14159265358979 * r * r * 1000.0;
}

inline double defaultSnowSigmaPerKm()
{
	const WwPrecip::TypeInfo & ti = WwPrecip::typeInfo( WwPrecip::SnowFine );
	return sigmaPerKmOf( WwPrecip::SnowFine, ti.sizeMm, ti.rateMmH );
}

/* lane RAINFOG2: this frame's rain and snow shares, each weather weighted as CLOUD6e's share() (pct the new one, 1 - pct
 * the leaving one) and by its rate scale (gl/precipmath.h spgdRateScale: the SPGD's density / vanilla's 7). type[i]:
 * -2 no precipitation shader, -1 not read (rain), 0 rain, 1 snow; snowOn false (WW_CLOUD_SNOWFOG off / its red) counts a
 * snow shader as rain, as CLOUD6e did. scale 1 and no snow: rain == share( ... ) exactly. */
inline void shares( const int type[2], const double scale[2], bool blending, double pct, bool snowOn, double & rain, double & snow )
{
	rain = snow = 0.0;
	for ( int i = 0; i < ( blending ? 2 : 1 ); i++ ) {
		const double w = blending ? WwPrecip::blendWeight( i == 1, pct ) : 1.0;
		int k = WwPrecip::spgdKind( type[i] );
		if ( k == WwPrecip::SpgdSnow && !snowOn )
			k = WwPrecip::SpgdRain;
		if ( k == WwPrecip::SpgdRain )
			rain += w * scale[i];
		else if ( k == WwPrecip::SpgdSnow )
			snow += w * scale[i];
	}
}

//! the height factor at hKm below the base (negative above it)
inline double profile( double hKm )
{
	if ( hKm >= 0.0 )
		return 1.0 + kBaseExcess * std::exp( -hKm / kFalloffKm );
	return ( 1.0 + kBaseExcess ) * std::max( 1.0 + hKm / kAboveKm, 0.0 );
}

//! the share of rain this frame: the current weather's weight when it rains, plus the leaving one's
inline double share( bool curRains, bool oldRains, bool blending, double pct )
{
	if ( !blending )
		return curRains ? 1.0 : 0.0;
	return ( curRains ? WwPrecip::blendWeight( false, pct ) : 0.0 ) + ( oldRains ? WwPrecip::blendWeight( true, pct ) : 0.0 );
}

//! the switch WW_CLOUD_RAINFOG: on unless "0", "off", "false" or "no" (trimmed, lower case; unset = on)
inline bool switchOn( const char * v )
{
	if ( !v )
		return true;
	for ( const char * off : { "0", "off", "false", "no" } )
		if ( std::strcmp( v, off ) == 0 )
			return false;
	return true;
}

//! the red WW_CLOUD_RAINFOG_RED: "1", "on" or "old" draws today's air (no rain fog)
inline bool redOn( const char * v )
{
	if ( !v )
		return false;
	for ( const char * on : { "1", "on", "old" } )
		if ( std::strcmp( v, on ) == 0 )
			return true;
	return false;
}

}	// namespace WwRainFog

#endif
