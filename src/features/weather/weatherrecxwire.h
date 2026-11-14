#ifndef WW_WEATHERRECXWIRE_H
#define WW_WEATHERRECXWIRE_H

/* lane WEATHERRECX4 (SPEC.md 1.8 "Read by the drawing", notes UNREAL_GAPS rows 2, 3, 4, 5/F8, 9/F3, 13, F4, F7, F9, F14,
 * F15): how each wired weather key reaches the term the picture already draws. Plain C++ (no Qt), so the offline gate
 * (tests/features/weather/weatherrecx3/dump.cpp) prints the same drawn terms the consumers use. Every function takes
 * today's value and the file's; an unset key (< 0, occlusion -1) returns today's value untouched (a branch, never a
 * multiply by 1), so a weather whose file does not say the key draws exactly as before. */

#include <algorithm>

namespace WwRecxWire
{

constexpr float kMetresPerUnit = 0.0142875f;	//!< one game unit in metres (atmomath.h kUnitKm x 1000)

//! atmosphere.fogStart (m): the record fog's near distance (FNAM near), game units
inline float fogNear( float todayUnits, float startM )
{
	return startM >= 0.0f ? startM / kMetresPerUnit : todayUnits;
}
//! atmosphere.fogCutoff (m): the record fog's far distance (FNAM far, where it reaches its max), game units
inline float fogFar( float todayUnits, float cutoffM )
{
	return cutoffM >= 0.0f ? cutoffM / kMetresPerUnit : todayUnits;
}
//! atmosphere.fogMaxOpacity (share 0..1): the record fog's max (FNAM max)
inline float fogMax( float today, float maxOpacity )
{
	return maxOpacity >= 0.0f ? maxOpacity : today;
}
//! atmosphere.volumetric.extinctionScale (x): the near air's (froxel) extinction, any unit
inline float volExtinction( float todaySigma, float scale )
{
	return scale >= 0.0f ? todaySigma * scale : todaySigma;
}
//! atmosphere.volumetric.viewDistance (m): where the froxel air ends (never past today's end), in the caller's units
inline float volViewDistance( float todayFar, float viewM, float unitsPerM )
{
	return viewM >= 0.0f ? std::min( todayFar, viewM * unitsPerM ) : todayFar;
}
//! atmosphere.multiScattering (x): the sky's multiple-scattering term (today 1, 0 under the red noms; the red wins)
inline float multiScatter( float today, float scale )
{
	return scale >= 0.0f && today > 0.0f ? scale : today;
}
//! atmosphere.localFogScale (x): every airzone volume's fog density (scale and add)
inline float localFog( float todayDensity, float scale )
{
	return scale >= 0.0f ? todayDensity * scale : todayDensity;
}
//! precipitation.fogExtinctionScale (x): the rain / snow fog's extinction (1 = the rate law)
inline double precipFog( double todaySigma, float scale )
{
	return scale >= 0.0f ? todaySigma * double( scale ) : todaySigma;
}
/*! atmosphere.sunIlluminance (lux): the sun's illuminance at normal incidence on the ground with the sun at the zenith in
 *  the clear-day reference air. The factor on the top-of-atmosphere sun = file / (E0 x T_zenith(clear day)); every sun
 *  angle and every haze then follows from the air as today. 1 when unset or the reference is not known. */
inline float sunFactor( float fileLux, float e0Lux, float tZenithClear )
{
	const float ref = e0Lux * tZenithClear;
	return fileLux >= 0.0f && ref > 0.0f ? fileLux / ref : 1.0f;
}
//! precipitation.occlusion (bool; -1 unset): false = the wet cover test is off (rain wets what stands under a roof too)
inline bool occlusion( bool today, int file )
{
	return file < 0 ? today : ( today && file != 0 );
}
//! night.moonGameFactor (x 1..20): the env pin WW_MOON_GAME wins, then the file, then the derived factor
inline float moonGame( float derived, float file, bool pinned )
{
	return pinned ? derived : file >= 1.0f ? file : derived;
}
//! night.adaptFloor (cd/m2 0.001..1): the camera's lowest adaptation luminance (today 1)
inline float adaptFloor( float todayCdm2, float file )
{
	return file > 0.0f ? file : todayCdm2;
}

}	// namespace WwRecxWire

#endif
