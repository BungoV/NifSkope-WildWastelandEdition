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

//! split a total of rain + snow (mm/h water) at t: rain, snow
inline void split( float total, float t, float & rain, float & snow )
{
	const float f = snowShare( t );
	snow = total * f;
	rain = total - snow;
}

}	// namespace WwAirTemp

#endif
