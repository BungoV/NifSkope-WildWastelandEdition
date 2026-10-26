#ifndef WW_FXPARITYRULES_H
#define WW_FXPARITYRULES_H

/* lane FXPARITY1: the game's particle modifier rules as pure functions, read from Todd's treat.
 * src/gl/controllers.cpp runs them while "Particle Game Rules" is on (fxparity.h); tests/fxparity checks them. */

#include <algorithm>
#include <cmath>

namespace FxRules
{

inline float lerp( float a, float b, float t )
{
	return ( b - a ) * t + a;
}

/* BSPSysSimpleColorModifier. f = the six percents in file order: fade in, fade out, colour 1 end, colour 1 start
 * (where colour 2 is reached), colour 2 end, colour 2 start (where colour 3 is reached). col = the three RGBA
 * colours, u = age / life span. The alpha has its own ramps: colour 1's alpha into colour 2's over the fade in,
 * colour 2's into colour 3's after the fade out; a fade of 0 means none. */
inline void simpleColor( const float f[6], const float col[3][4], float u, float out[4] )
{
	const float fi = f[0], fo = f[1], a = f[2], b = f[3], c = f[4], d = f[5];
	if ( b - a == 0.0f || u >= b ) {
		if ( d - c == 0.0f || u <= c ) {
			for ( int i = 0; i < 3; i++ )
				out[i] = col[1][i];
		} else if ( u <= d ) {
			const float t = ( u - c ) / ( d - c );
			for ( int i = 0; i < 3; i++ )
				out[i] = lerp( col[1][i], col[2][i], t );
		} else {
			for ( int i = 0; i < 3; i++ )
				out[i] = col[2][i];
		}
	} else if ( u < a ) {
		for ( int i = 0; i < 3; i++ )
			out[i] = col[0][i];
	} else {
		const float t = ( u - a ) / ( b - a );
		for ( int i = 0; i < 3; i++ )
			out[i] = lerp( col[0][i], col[1][i], t );
	}
	if ( fo == 0.0f || u <= fo )
		out[3] = ( fi == 0.0f || u >= fi ) ? col[1][3] : lerp( col[0][3], col[1][3], u / fi );
	else
		out[3] = lerp( col[1][3], col[2][3], ( u - fo ) / ( 1.0f - fo ) );
}

/* NiPSysDragModifier: the share of the velocity along the drag axis removed this step. Linear in the step and
 * capped at all of it (not an exponential decay). */
inline float dragShare( float percentage, float dt )
{
	return std::min( percentage * 30.0f * dt, 1.0f );
}

/* lane FXPARITY2: the drag's range. d = the particle's distance from the Drag Object: full inside Range, none at or
 * past Range Falloff, linear between. Multiplies the percentage before dragShare. */
inline float dragRangeTerm( float d, float range, float falloff )
{
	if ( d <= range )
		return 1.0f;
	if ( d >= falloff )
		return 0.0f;
	return 1.0f - ( d - range ) / ( falloff - range );
}

inline float dot3( const float a[3], const float b[3] )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/* lane FXPARITY2: NiPSysBombModifier, the velocity change of one particle over dt. r = position - the Bomb Object's
 * position, axis = its rotated, normalised axis (psys space). decayType 0 none / 1 linear / 2 exponential, symmetry
 * 0 spherical / 1 cylindrical / 2 planar. With a decay type, nothing past Decay. */
inline void bombDelta( const float r[3], const float axis[3], float decay, float deltaV, int decayType, int symmetry,
	float dt, float out[3] )
{
	out[0] = out[1] = out[2] = 0.0f;
	const float len = std::sqrt( dot3( r, r ) );
	if ( decayType != 0 && len > decay )
		return;
	float dir[3] = { 0, 0, 0 }, dist = 0.0f;
	if ( symmetry == 2 ) {
		const float p = dot3( r, axis );
		const float s = p < 0.0f ? -1.0f : 1.0f;
		for ( int i = 0; i < 3; i++ )
			dir[i] = axis[i] * s;
		dist = p * s;
	} else {
		float v[3] = { r[0], r[1], r[2] };
		if ( symmetry == 1 ) {
			const float p = dot3( r, axis );
			for ( int i = 0; i < 3; i++ )
				v[i] -= axis[i] * p;
		}
		dist = std::sqrt( dot3( v, v ) );
		if ( dist > 0.0f )
			for ( int i = 0; i < 3; i++ )
				dir[i] = v[i] / dist;
	}
	float f = 1.0f;
	if ( decayType == 1 )
		f = decay > 0.0f ? ( decay - dist ) / decay : 0.0f;
	else if ( decayType == 2 )
		f = decay > 0.0f ? std::exp( -dist / decay ) : 0.0f;
	const float k = f * deltaV * std::max( dt, 0.0f );
	for ( int i = 0; i < 3; i++ )
		out[i] = dir[i] * k;
}

/* lane FXPARITY2: NiPSysGravityModifier, the velocity change of one particle over dt. The strength is scaled by 1.6.
 * Planar: along the axis, decay exp(-Decay x |axis . (centre - pos)|). Spherical: TOWARD the gravity object's centre,
 * decay exp(-Decay x |centre - pos|). turb = three randoms in [-1, 1] times Turbulence x Turbulence Scale x 500,
 * added before the dt multiply (pass zeros when Turbulence is 0). */
inline void gravityDelta( const float pos[3], const float centre[3], const float axis[3], float strength, float decay,
	bool spherical, const float turb[3], float dt, float out[3] )
{
	const float c[3] = { centre[0] - pos[0], centre[1] - pos[1], centre[2] - pos[2] };
	float dir[3] = { axis[0], axis[1], axis[2] };
	float dist = 0.0f;
	if ( spherical ) {
		dist = std::sqrt( dot3( c, c ) );
		for ( int i = 0; i < 3; i++ )
			dir[i] = dist > 0.0f ? c[i] / dist : 0.0f;
	} else {
		dist = std::fabs( dot3( axis, c ) );
	}
	const float f = decay != 0.0f ? std::exp( -decay * dist ) : 1.0f;
	const float s = strength * 1.6f * f, t = std::max( dt, 0.0f );
	for ( int i = 0; i < 3; i++ )
		out[i] = ( dir[i] * s + turb[i] ) * t;
}

/* BSPSysSubTexModifier: the flipbook advances one frame each time int(u * steps) grows, so Frame Count is the
 * number of steps over the particle's life, not a rate. k starts at -1; rnd(x) is a random value in [0, x]. */
struct SubTex
{
	float start = 0, startFudge = 0, end = 0, loopStart = 0, loopFudge = 0, count = 0, countFudge = 0;
};

template <typename Rnd>
inline void subTexStep( const SubTex & s, int & k, int & steps, int & frame, float u, Rnd && rnd )
{
	if ( k == -1 )
		steps = int( s.count + rnd( s.countFudge ) );
	const int now = int( u * float( steps ) );
	if ( now <= k )
		return;
	const bool first = k == -1;
	k = now;
	if ( first )
		frame = int( s.start + rnd( s.startFudge ) );
	else if ( float( frame + 1 ) > s.end )
		frame = int( s.loopStart + rnd( s.loopFudge ) );
	else
		frame++;
}

}	// namespace FxRules

#endif
