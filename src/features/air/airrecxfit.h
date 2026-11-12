#ifndef WW_AIRRECXFIT_H
#define WW_AIRRECXFIT_H

/* lane AIRPERF1 A8 (bungo 11-11): the air under a weather entry's "physicalWeather" switch (.recx spec). With the
 * switch on, the entry's physical values alone decide the part they cover, and the weather record is no fit target,
 * clamp, floor or blend for it:
 *   atmosphere.visibility  -> the air's base density: Koschmieder, 3.912 / V per km less clear air's (the same
 *                             relation the sky's haze takes, weatherrecxmath.h hazeFromVisibility)
 *   groundFog.strength     -> the layer's density at its floor as a multiple of that base, ADDED on top
 *   groundFog.thickness    -> the layer falls by e per thickness units of height
 * so at the layer's floor the density is base x (1 + strength). Std only: the g++ unit is
 * notes/modules1/airperf1/unit/recxfit.cpp. */

#include "features/weather/weatherrecxmath.h"

#include <algorithm>
#include <cmath>

namespace airrecx
{

constexpr float kReachUnits = 262144.0f;	// the grids' longest reach (3.75 km): an engine limit, not a weather value

//! the base density per game unit from the file's visibility (metres)
inline float baseSigmaPerUnit( float visM, float clearAirPerKm, float unitsPerKm )
{
	return WwWeatherRecxMath::hazeFromVisibility( visM, clearAirPerKm ) / unitsPerKm;
}

//! where the base air alone reaches 99 % opacity, at most the grids' reach
inline float reach( float sigmaPerUnit )
{
	return sigmaPerUnit > 0.0f ? std::min( kReachUnits, 4.6051702f / sigmaPerUnit ) : kReachUnits;
}

/*! the factor on the base density when a ground layer of `strength` joins air of relative density rb at its floor:
 *  onTop (the switch on): 1, the layer is added; else rb / (rb + strength), the floor keeps the density it had */
inline float layerBaseFactor( float rb, float strength, bool onTop )
{
#ifdef AIRRECX_RED_KEEP
	onTop = false;
#endif
	if ( onTop || !( strength > 0.0f ) || !( rb > 0.0f ) )
		return 1.0f;
	return rb / ( rb + strength );
}

//! the density at height dz above the layer's floor, per unit (even base air, the layer on it)
inline float sigmaAt( float basePerUnit, float strength, float thickness, float dz )
{
	const float lay = strength > 0.0f && thickness > 0.0f ? strength * std::min( 1.0f, std::exp( -dz / thickness ) ) : 0.0f;
	return basePerUnit * ( 1.0f + lay );
}

}

#endif
