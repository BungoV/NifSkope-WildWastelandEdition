#ifndef WW_WEATHERRECXMATH_H
#define WW_WEATHERRECXMATH_H

/* lane WX1: the weather .recx blocks' math (atmosphere / clouds / night), no GL and no Qt, shared by
 * gl/weatherrecx.cpp, gl/atmosphere.cpp, gl/cloudmap.cpp and the offline dump (tests/weatherrecx/wxdump.cpp).
 * Spec: FO4-Extension-Formats recx/SPEC.md "atmosphere, clouds, night (Weathers)". */

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace WwWeatherRecxMath
{

// the spec's ranges (clamped with a "clamped" warning)
constexpr float kVisMin = 100.0f, kVisMax = 400000.0f;	// atmosphere.visibility, m
constexpr float kHazeHMin = 20.0f, kHazeHMax = 20000.0f;	// atmosphere.hazeHeight, m
constexpr float kOzoneMax = 4.0f;
constexpr float kDensityMax = 10.0f, kNightMax = 10.0f;
constexpr float kLayerMin = 600.0f, kLayerMax = 20000.0f;	// clouds.layers.<n>.base/top, metres above sea level

/* Koschmieder: a dark object fades to 2 % contrast at V, so the ground extinction is -ln(0.02) / V = 3.912 / V;
 * the haze is what clear air does not already give (atmomath.h hazeFromFog's rung, same clamp) */
inline float hazeFromVisibility( float visM, float clearAirPerKm )
{
	const float vKm = std::clamp( visM, kVisMin, kVisMax ) / 1000.0f;
	return std::clamp( 3.912023f / vKm - clearAirPerKm, 0.0f, 50.0f );
}

//! the record rung's visibility for the census: where the fog reaches Max at Far (game units -> m)
inline float visibilityOfFog( float fogFarUnits, float fogMax )
{
	if ( fogFarUnits <= 0.0f || fogMax <= 0.0f )
		return 0.0f;
	return fogFarUnits * 0.0142875f * 3.912023f / -std::log( 1.0f - std::min( fogMax, 0.98f ) );
}

//! 1 - prod(1 - c): the sky the three bands cover together
inline float union3( const float c[3] )
{
	double keep = 1.0;
	for ( int b = 0; b < 3; b++ )
		keep *= 1.0 - double( std::clamp( c[b], 0.0f, 1.0f ) );
	return float( 1.0 - keep );
}

/* clouds.coverage: one factor k on every band's cover so that their union is `target`; each band keeps its share.
 * A weather with no cover stays clear (there is nothing to scale). Bisection: the union grows with k. */
inline void coverTo( const float c[3], float target, float out[3] )
{
	target = std::clamp( target, 0.0f, 1.0f );
	float mx = 0.0f;
	for ( int b = 0; b < 3; b++ ) {
		out[b] = c[b];
		mx = std::max( mx, c[b] );
	}
	if ( mx <= 0.0f )
		return;
	auto at = [&]( double k, float o[3] ) {
		for ( int b = 0; b < 3; b++ )
			o[b] = float( std::min( 1.0, k * double( std::max( c[b], 0.0f ) ) ) );
	};
	double lo = 0.0, hi = 1.0 / double( mx );	// at hi the largest band is full: union 1
	for ( int i = 0; i < 60; i++ ) {
		const double mid = 0.5 * ( lo + hi );
		float o[3];
		at( mid, o );
		( union3( o ) < target ? lo : hi ) = mid;
	}
	at( 0.5 * ( lo + hi ), out );
}

/* clouds.coverage / clouds.density on the three bands (cloudmap.cpp paramsOf); < 0 = not set. recordFirst (the red):
 * the record's cover wins when it has one (the wrong order) */
inline void cloudsFromFile( float cover[3], float density[3], float coverage, float densityK, bool recordFirst )
{
	const bool recordHas = cover[0] > 0.0f || cover[1] > 0.0f || cover[2] > 0.0f;
	if ( coverage >= 0.0f && !( recordFirst && recordHas ) ) {
		float o[3];
		coverTo( cover, coverage, o );
		for ( int b = 0; b < 3; b++ )
			cover[b] = o[b];	// the sheet share is a share of the band's cover: it stays
	}
	if ( densityK >= 0.0f )
		for ( int b = 0; b < 3; b++ )
			density[b] *= densityK;
}

/* a layer's base/top (metres above sea level) as the bands' heights above the eye (cloudmath.h roleInfo's frame);
 * the band stays above the eye: a camera above the base sees the band from its own height (fly-through: CLOUD4) */
inline void layerAboveEye( float baseAsl, float topAsl, float eyeM, float & baseM, float & topM )
{
	baseM = std::max( 0.0f, baseAsl - eyeM );
	topM = std::max( baseM + 50.0f, topAsl - eyeM );
}

/* `hours`: linear between the keyed hours around the clock; one key = that value all day; no key = base */
inline double hoursAt( std::vector<std::pair<double, double>> keys, double hour, double base )
{
	if ( keys.empty() )
		return base;
	for ( auto & k : keys )
		k.first = std::fmod( std::fmod( k.first, 24.0 ) + 24.0, 24.0 );
	std::sort( keys.begin(), keys.end() );
	if ( keys.size() == 1 )
		return keys[0].second;
	hour = std::fmod( std::fmod( hour, 24.0 ) + 24.0, 24.0 );
	for ( size_t i = 0; i < keys.size(); i++ ) {
		const auto & a = keys[i];
		const auto & b = keys[( i + 1 ) % keys.size()];
		double span = b.first - a.first, t = hour - a.first;
		if ( span <= 0.0 )
			span += 24.0;
		if ( t < 0.0 )
			t += 24.0;
		if ( t <= span )
			return a.second + ( b.second - a.second ) * ( t / span );
	}
	return base;
}

}	// namespace WwWeatherRecxMath

#endif
