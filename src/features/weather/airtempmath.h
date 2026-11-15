#ifndef WW_AIRTEMPMATH_H
#define WW_AIRTEMPMATH_H

/* lane WEATHERRECX2 (SPEC.md 1.8 temperature.air): what the air temperature does to precipitation and wetness.
 * Plain C++ (no Qt), so the offline gate reads the same rule.
 *  - Phase: the snow share of the precipitation at the ground, Dai (2008, GRL 35 L12802) land fit
 *    F = a (tanh(b (T - c)) - d) percent, a -48.2292, b 0.7205, c 1.1662, d 1.0223: 50 % near 1.1 C, under 2 % above 4 C,
 *    over 95 % below -1.5 C (the fit's ends stretched to 0 and 1). The mixed band is what a forecaster calls sleet; the viewer draws it as rain + snow layers
 *    (sleet has no type of its own since PRECIPLOOK1).
 *  - Drying: evaporation follows the saturation vapour pressure (Tetens: e_s = 0.6108 exp(17.27 T / (T + 237.3)) kPa),
 *    so the file's drying time is at 15 C and scales by e_s(15) / e_s(T) (clamped 0.25..8).
 *  - Wetting: snow does not soak a surface the way rain does: the wetting time is divided by the rain share
 *    (at most 20 x slower). */

#include <algorithm>
#include <cmath>

namespace WwAirTemp
{

constexpr float kNotSet = -999.0f;	// WwWeatherRecx::airTemp <= this = not set

//! the snow share 0..1 of precipitation reaching the ground at air temperature t (deg C)
inline float snowShare( float t )
{
	// the fit's own ends are 1.1 % and 97.5 % (station mixes); stretched to 0..1 so deep cold is all snow
	const double a = -48.2292, b = 0.7205, c = 1.1662, d = 1.0223;
	const double f = a * ( std::tanh( b * ( double( t ) - c ) ) - d ), lo = a * ( 1.0 - d ), hi = a * ( -1.0 - d );
	return float( std::clamp( ( f - lo ) / ( hi - lo ), 0.0, 1.0 ) );
}

//! saturation vapour pressure over water, kPa (Tetens)
inline double vapourKPa( double t )
{
	return 0.6108 * std::exp( 17.27 * t / ( t + 237.3 ) );
}

//! the factor on the drying time at t (1 at 15 C)
inline float dryingFactor( float t )
{
	return float( std::clamp( vapourKPa( 15.0 ) / vapourKPa( double( t ) ), 0.25, 8.0 ) );
}

//! the factor on the wetting time at t (1 when it all falls as rain)
inline float wettingFactor( float t )
{
	return 1.0f / std::max( 1.0f - snowShare( t ), 0.05f );
}

//! the file's minutes at t (<= 0 stays: the game's speed)
inline float dryingMinutes( float minutes, float t )
{
	return minutes > 0.0f && t > kNotSet ? minutes * dryingFactor( t ) : minutes;
}
inline float wettingMinutes( float minutes, float t )
{
	return minutes > 0.0f && t > kNotSet ? minutes * wettingFactor( t ) : minutes;
}

/* lane WEATHERRECX4: drying from temperature, humidity, wind and sun (replaces the temperature-only factor above).
 * Penman (1948) in the SI form of Shuttleworth (1993):
 *   E = ( m Rn + g 6.43 (1 + 0.536 u2) de ) / ( lambda (m + g) )   mm/day
 *   m = 4098 e_s / (T + 237.3)^2 kPa/K (the slope of Tetens), g = 0.0016286 P / lambda = 0.0673 kPa/K at P 101.3 kPa,
 *   lambda 2.45 MJ/kg (FAO-56), de = (1 - RH) e_s kPa, Rn in MJ m-2 day-1 (W/m2 x 0.0864), u2 the 2 m wind, at least
 *   0.5 m/s (FAO-56 advice), from the file's wind.speed (10 m) by the spec's log law speed x ln(2/0.3) / ln(10/0.3).
 * Sun: clear-sky shortwave Rso = 0.75 x 1361 W/m2 x sin(sun height) (FAO-56 Eq. 37 at sea level), cloud cover N cuts
 * it by (1 - 0.75 N^3.4) (Kasten and Czeplak 1980), net shortwave = 0.77 Rs (FAO-56 albedo 0.23); the net longwave is
 * left out.
 * REFERENCE CONDITION: the file's drying hours are the time at 15 C, RH 60 %, wind.speed 2 m/s (10 m), clear sky, sun
 * 45 degrees high. The time at another condition is hours x E_ref / E (clamped 0.25..8, so still saturated air at night is 8x).
 * An unset input takes its reference value. */
constexpr float kDryRefT = 15.0f, kDryRefRh = 0.6f, kDryRefWind = 2.0f, kDryRefSinSun = 0.70710678f, kDryRefCover = 0.0f;

struct DryingAir
{
	float t = kNotSet;	//!< deg C; <= kNotSet = reference
	float rh = -1.0f;	//!< 0..1; < 0 = reference
	float wind = -1.0f;	//!< m/s 10 m above the ground (the file's wind.speed); < 0 = reference
	float sinSun = -2.0f;	//!< sine of the sun's height; < -1 = reference
	float cover = -1.0f;	//!< cloud cover 0..1; < 0 = reference
};

//! net radiation W/m2 for the sun's height and the cloud cover (0 with the sun down)
inline double dryingNetRadiationW( double sinSun, double cover )
{
	const double rso = 0.75 * 1361.0 * std::max( sinSun, 0.0 );
	const double rs = rso * ( 1.0 - 0.75 * std::pow( std::clamp( cover, 0.0, 1.0 ), 3.4 ) );
	return 0.77 * rs;
}

//! Penman open-water evaporation, mm/day
inline double penmanMmDay( double t, double rh, double u2, double rnW )
{
	const double lambda = 2.45, gamma = 0.0016286 * 101.3 / lambda;
	const double es = vapourKPa( t ), m = 4098.0 * es / ( ( t + 237.3 ) * ( t + 237.3 ) );
	const double de = ( 1.0 - std::clamp( rh, 0.0, 1.0 ) ) * es;
	const double rn = std::max( rnW, 0.0 ) * 0.0864;
	return ( m * rn + gamma * 6.43 * ( 1.0 + 0.536 * std::max( u2, 0.5 ) ) * de ) / ( lambda * ( m + gamma ) );
}

inline double penmanOf( const DryingAir & a )
{
	const double t = a.t > kNotSet ? a.t : kDryRefT, rh = a.rh >= 0.0f ? a.rh : kDryRefRh;
	const double u = ( a.wind >= 0.0f ? a.wind : kDryRefWind ) * std::log( 2.0 / 0.3 ) / std::log( 10.0 / 0.3 );	// 10 m -> 2 m
	const double s = a.sinSun >= -1.0f ? a.sinSun : kDryRefSinSun, c = a.cover >= 0.0f ? a.cover : kDryRefCover;
	return penmanMmDay( t, rh, u, dryingNetRadiationW( s, c ) );
}

//! the factor on the file's drying time: E at the reference over E now (1 with every input at the reference)
inline float dryingFactorAir( const DryingAir & a )
{
	const double ref = penmanOf( DryingAir() ), e = penmanOf( a );
	return e > 0.0 ? float( std::clamp( ref / e, 0.25, 8.0 ) ) : 8.0f;
}

inline float dryingMinutesAir( float minutes, const DryingAir & a )
{
	return minutes > 0.0f ? minutes * dryingFactorAir( a ) : minutes;
}

//! split a total of rain + snow (mm/h water) at t: rain, snow
inline void split( float total, float t, float & rain, float & snow )
{
	const float f = snowShare( t );
	snow = total * f;
	rain = total - snow;
}

}	// namespace WwAirTemp

#endif
