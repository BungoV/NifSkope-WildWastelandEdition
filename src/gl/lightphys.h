#ifndef LIGHTPHYS_H
#define LIGHTPHYS_H

/* lane LIGHTPHYS1 (10-25): light physics for the cell's placed lights (falloff, advanced model, intensity curve, area
 * lights). Pure math (no Qt); res/shaders/cell_lights.glsl holds the GLSL twin line for line, tests/lightphys pins
 * this to values computed from the reference implementation. Every row Off = the game's curve, the same float.
 *   falloff  Off: pow(1 - saturate(scale x^exponent + bias), 2.2), x = d / r
 *            Physical: k saturate(1 - x^4)^2 / (x^2 + s^2), k fits the old curve at x = anchor (0.38164), s = 0.3
 *            Advanced: N / (0.9 - 2/3 x0) x window(x) / max(x0, x)^2, x0 = the minimum distance (0.05 m, at least
 *            0.2 m or 0.1 r) over r, window a smoothstep from 0.8 r; N = the vanilla curve's flux x calibration (1)
 *            Lights whose authored curve is no falloff (flat, lit at its own edge, unfaded at the anchor) keep theirs.
 *   intensity  colour x pow(luminance / pivot, curve - 1), pivot 0.1043; curve 1 = no change
 *   area     PBR specular only: the representative point of a sphere of min(8, 0.3 r) clamped 2..32 units,
 *            alpha widened by size / 2d (capped 0.9999) */

#include <algorithm>
#include <cmath>

namespace wwlp {

constexpr float kAnchor = 0.38164f;
constexpr float kSourceFraction = 0.30f;
constexpr float kPivot = 0.1043f;
constexpr float kSourceWorld = 8.0f;
constexpr float kSourceWorldMin = 2.0f;
constexpr float kSourceWorldMax = 32.0f;
constexpr float kVisibleFloor = 1.0f / 255.0f;
constexpr float kAdvMinMetres = 0.05f;
constexpr float kAdvCalibration = 1.0f;
constexpr float kUnitsPerMetre = 69.99f;
constexpr float kAdvWindowStart = 0.8f;
constexpr float kWidenedAlphaMax = 0.9999f;

enum Mode : int { Vanilla = 0, Physical = 1, Advanced = 2 };

struct Params
{
	int mode = Vanilla;
	float anchor = kAnchor;
	float sourceFraction = kSourceFraction;
	float advNumerator = 0.0f;	//!< vanillaFlux() x calibration
	float advMinMetres = kAdvMinMetres;
	float curve = 1.0f;			//!< intensity exponent; exactly 1 = off
	float pivot = kPivot;
	float areaWorld = 0.0f;		//!< emitter size in units; 0 = point lights
	int red = 0;				//!< 1 noanchor (k = 1), 2 nowindow (advanced), 4 nowiden (area), 8 nopivot (pivot 1)
};

inline float sat( float v )
{
	return v > 0.0f ? ( v < 1.0f ? v : 1.0f ) : 0.0f;	// NaN -> 0
}

//! the game's curve (cell_lights.glsl cellRadial without the linear red)
inline float attVanilla( float x, float bias, float scale, float exponent )
{
	x = sat( x );
	const float xe = exponent > 0.0f ? std::pow( x, exponent ) : 1.0f;
	return std::pow( 1.0f - sat( scale * xe + bias ), 2.2f );
}

inline float physRaw( float x, float s )
{
	x = sat( x );
	const float x2 = x * x;
	const float w = sat( 1.0f - x2 * x2 );
	const float den = x2 + s * s;
	return den > 0.0f ? w * w / den : 0.0f;
}

//! an authored curve that is no falloff: flat, lit at its own edge, or brighter at the anchor than an ordinary bulb
inline bool notAFalloff( float anchor, float bias, float scale, float exponent )
{
	if ( !( exponent > 0.0f ) || !( sat( scale + bias ) > sat( bias ) ) )
		return true;
	if ( attVanilla( 1.0f, bias, scale, exponent ) >= kVisibleFloor )
		return true;
	const float a = sat( anchor ), base = 1.0f - a * a;
	const float ceil = base > 0.0f ? std::pow( base, 1.1f ) : 0.0f;
	return attVanilla( anchor, bias, scale, exponent ) > ceil;
}

inline float parityScale( float anchor, float s, float bias, float scale, float exponent )
{
	const float raw = physRaw( anchor, s );
	return raw > 0.0f ? attVanilla( anchor, bias, scale, exponent ) / raw : 0.0f;
}

//! 0.5 G(1.5) G(3.2) / G(4.7): the integral of (1 - x^2)^2.2 x^2 over 0..1
inline float vanillaFlux()
{
	return float( 0.5 * std::tgamma( 1.5 ) * std::tgamma( 3.2 ) / std::tgamma( 4.7 ) );
}

inline float advMinFraction( float minMetres, float r )
{
	if ( !( r > 0.0f ) )
		return 0.5f;
	const float nearM = std::max( std::max( minMetres, 0.001f ), std::max( 0.1f * r / kUnitsPerMetre, 0.2f ) );
	return std::clamp( nearM * kUnitsPerMetre / r, 1.0e-4f, 0.5f );
}

inline float advWindow( float x )
{
	const float t = std::clamp( ( x - kAdvWindowStart ) / ( 1.0f - kAdvWindowStart ), 0.0f, 1.0f );
	return 1.0f - t * t * ( 3.0f - 2.0f * t );
}

inline float advAtt( float x, float r, float k, float minMetres, bool noWindow = false )
{
	x = sat( x );
	const float x0 = advMinFraction( minMetres, r );
	const float den = 0.5f * ( 1.0f + kAdvWindowStart ) - ( 2.0f / 3.0f ) * x0;
	const float m = std::max( x0, x );
	return den > 0.0f ? ( k / den ) * ( noWindow ? 1.0f : advWindow( x ) ) / ( m * m ) : 0.0f;
}

//! the radial term at distance d of a light of radius r and authored curve (bias, scale, exponent)
inline float falloff( const Params & p, float d, float r, float bias, float scale, float exponent )
{
	const float x = sat( d / std::max( r, 0.001f ) );
	const float van = attVanilla( x, bias, scale, exponent );
	if ( p.mode == Vanilla || notAFalloff( p.anchor, bias, scale, exponent ) )
		return van;
	if ( p.mode == Advanced )
		return advAtt( x, r, p.advNumerator, p.advMinMetres, ( p.red & 2 ) != 0 );
	const float k = ( p.red & 1 ) ? 1.0f : parityScale( p.anchor, p.sourceFraction, bias, scale, exponent );
	return k > 0.0f ? k * physRaw( x, p.sourceFraction ) : van;
}

//! the colour multiplier of a light of linear colour c
inline float intensityScale( const Params & p, const float c[3] )
{
	if ( p.curve == 1.0f )
		return 1.0f;
	const float lum = 0.2126f * std::max( c[0], 0.0f ) + 0.7152f * std::max( c[1], 0.0f ) + 0.0722f * std::max( c[2], 0.0f );
	const float pivot = ( p.red & 8 ) ? 1.0f : p.pivot;
	if ( !( lum > 0.0f ) || !( pivot > 0.0f ) )
		return 1.0f;
	const float s = std::pow( lum / pivot, p.curve - 1.0f );
	return std::isfinite( s ) ? s : 1.0f;
}

//! the emitter's size in units (the size, capped by the fraction of the reach)
inline float areaSize( float r, float fraction, float world )
{
	if ( !( r > 0.0f ) )
		return kSourceWorldMin;
	return std::clamp( std::min( world, r * fraction ), kSourceWorldMin, kSourceWorldMax );
}

inline float areaWidenedRough( float rough, float d, float size )
{
	const float a = std::min( std::max( rough * rough, 1e-4f ) + size / std::max( 2.0f * d, 1e-4f ), kWidenedAlphaMax );
	return std::sqrt( sat( a ) );
}

}	// namespace wwlp

#endif // LIGHTPHYS_H
