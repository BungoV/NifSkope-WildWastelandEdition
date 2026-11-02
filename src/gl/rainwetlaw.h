#pragma once

/* lane FO76WEATHER1: rain wetness on every material, Fallout 76's law as its deferred PBR shaders apply it.
 * Porosity comes from the smoothness (or the material's custom porosity); metals never wet. A wet pixel darkens and
 * gains smoothness; its normal and F0 stay. res/shaders/rain_wet.glsl is the same law on the GPU.
 * The clock is Fallout 4's (src/landlodspec.h wetStep), run toward the rain's share of the weather (a change blends). */

#include <algorithm>

namespace WwRainWet
{
constexpr float kWetSpeed = 0.03f;	// fWetSpeed: per second while it rains
constexpr float kDrySpeed = 0.02f;	// fDrySpeed: per second while it does not

inline float sat( float x )
{
	return std::clamp( x, 0.0f, 1.0f );
}

//! 1 at smoothness 0.1 and below, 0 at 0.7 and above; the material's own value when useCustom is 1
inline float porosity( float smooth, float custom = 0.0f, float useCustom = 0.0f )
{
	const float fromSmooth = 1.1666f - 1.666f * smooth;
	return sat( fromSmooth + ( custom - fromSmooth ) * useCustom );
}

//! 1 where the reflectance (F0) averages above about 0.5: a metal, never wet
inline float metalMask( float f0r, float f0g, float f0b )
{
	return sat( ( f0r + f0g + f0b ) * 0.33f * 1000.0f - 500.0f );
}

//! the wetness of one pixel: the weather's amount, its exposure to the sky (1 = open), porosity, not metal
inline float wetness( float amount, float exposed, float por, float metal )
{
	return amount * exposed * por * ( 1.0f - metal );
}

//! the albedo's factor (linear colour)
inline float albedoK( float w )
{
	return 1.0f - 0.8f * w;
}

inline float wetSmooth( float smooth, float w )
{
	return 1.0f - ( 1.0f - 0.4f * w ) * ( 1.0f - smooth );
}

//! one step of the clock toward target; target 1 or 0 is landlodspec.h's wetStep exactly
inline float step( float wet, float target, float dt )
{
	target = sat( target );
	if ( wet < target )
		return std::min( wet + kWetSpeed * dt, target );
	return std::max( wet - kDrySpeed * dt, target );
}
}	// namespace WwRainWet
