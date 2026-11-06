#pragma once

/* lane FO76WEATHER1: rain wetness on every material, Fallout 76's law as its deferred PBR shaders apply it.
 * Porosity comes from the smoothness (or the material's custom porosity); metals never wet. A wet pixel darkens and
 * gains smoothness; its normal and F0 stay. res/shaders/features/weather/rain_wet.glsl is the same law on the GPU.
 * The clock is Fallout 4's (src/features/far-ground/landlodspec.h wetStep), run toward the rain's share of the weather (a change blends). */

#include <algorithm>
#include <cmath>

#include "features/weather/preciproof.h"

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

/* lane WETSNAP1: Fallout 4's wetness update ends each step with a snap: the wetness is 0 at once indoors, or
 * outdoors when neither weather of the change (the current one, and the leaving one while a change runs) is rain. So a
 * rain -> clear change dries at fDrySpeed only while it runs and ends dry. snapOn false = the old slow dry
 * (WW_CELL_RAINWET_RED=nosnap). */
inline bool snaps( bool rainNow, bool rainPrev, bool exterior )
{
	return !exterior || ( !rainNow && !rainPrev );
}

//! one frame: step toward target, then the snap
inline float update( float wet, float target, float dt, bool rainNow, bool rainPrev, bool exterior, bool snapOn )
{
	wet = step( wet, target, dt );
	return snapOn && snaps( rainNow, rainPrev, exterior ) ? 0.0f : wet;
}

/* lane WEATHERRECX1: the weather .recx's precipitation.wetting / drying (game minutes from dry to soaked and back,
 * SPEC.md 1.3) as the clock's per-second speeds: the game clock runs at timescale 20, so a minute is 3 real seconds.
 * wetting 15 = 0.022 / s (the game's fWetSpeed 0.03 is 11 minutes); <= 0 keeps the game's speed */
constexpr float kTimescale = 20.0f;
inline float perSecond( float minutes, float gameSpeed )
{
	return minutes > 0.0f ? kTimescale / ( 60.0f * minutes ) : gameSpeed;
}

inline float update( float wet, float target, float dt, bool rainNow, bool rainPrev, bool exterior, bool snapOn,
	float wettingMin, float dryingMin )
{
	target = sat( target );
	const float up = perSecond( wettingMin, kWetSpeed ), down = perSecond( dryingMin, kDrySpeed );
	wet = wet < target ? std::min( wet + up * dt, target ) : std::max( wet - down * dt, target );
	return snapOn && snaps( rainNow, rainPrev, exterior ) ? 0.0f : wet;
}

/* lane FO76WEATHER2: the sky exposure. Fallout 76's wet pixel shaders test a top-down precipitation depth map
 * (comparison sampler, so a 2 x 2 filtered test): a pixel with something above it stays dry. Here the map is
 * INTSKY1's roof map (preciproof.h WwRoofMap: each texel the highest z over it), built over the loaded cell's
 * solid shapes without the terrain. The test point steps 1.5 texels out along the surface's horizontal normal (a wall
 * reads the air in front of it, a roof slope the texel downhill), and a texel counts open when its height is no more
 * than kCoverBias + a slope term above the point (a roof's own texel holds the roof). Off the map = open. */
constexpr float kCoverBias = 8.0f;	// game units over the pixel a texel may stand and still count open
constexpr float kCoverStep = 1.5f;	// texels the test point steps out along the horizontal normal
constexpr float kCoverSlope = 2.0f;	// the slope term's cap: at most this many texels' rise

//! the extra height a texel of side `cell` may stand over a surface of normal n (unit) and still count open
inline float coverBias( float cell, float nx, float ny, float nz )
{
	const float h = std::sqrt( nx * nx + ny * ny );
	return kCoverBias + cell * std::min( h / std::max( std::fabs( nz ), 0.05f ), kCoverSlope );
}

//! 1 = open texel (off the map, nothing there, or low enough)
inline float coverTexelOpen( const WwRoofMap & m, int i, int j, float z, float bias )
{
	if ( !m.valid() || i < 0 || j < 0 || i >= m.nx || j >= m.ny )
		return 1.0f;
	return m.z[size_t( j ) * size_t( m.nx ) + size_t( i )] <= z + bias ? 1.0f : 0.0f;
}

//! the exposure of a pixel at p (world) with unit normal n: 1 open sky, 0 covered, between at a cover's edge (bilinear)
inline float exposed( const WwRoofMap & m, const float p[3], const float n[3] )
{
	if ( !m.valid() )
		return 1.0f;
	const float qx = ( p[0] + n[0] * kCoverStep * m.cell - m.x0 ) / m.cell - 0.5f;
	const float qy = ( p[1] + n[1] * kCoverStep * m.cell - m.y0 ) / m.cell - 0.5f;
	const int i = int( std::floor( qx ) ), j = int( std::floor( qy ) );
	const float fx = qx - float( i ), fy = qy - float( j );
	const float b = coverBias( m.cell, n[0], n[1], n[2] );
	const float e00 = coverTexelOpen( m, i, j, p[2], b ), e10 = coverTexelOpen( m, i + 1, j, p[2], b );
	const float e01 = coverTexelOpen( m, i, j + 1, p[2], b ), e11 = coverTexelOpen( m, i + 1, j + 1, p[2], b );
	return ( e00 + ( e10 - e00 ) * fx ) + ( ( e01 + ( e11 - e01 ) * fx ) - ( e00 + ( e10 - e00 ) * fx ) ) * fy;
}
}	// namespace WwRainWet
