#ifndef WW_CUBEFACECULL_H
#define WW_CUBEFACECULL_H

/* lane PERFFIX2 b (item 5b, 11-01): WHICH FACES OF A LIGHT'S DEPTH CUBE A CASTER CAN REACH.
 *
 * celllights.cpp draws every caster whose bounding sphere touches the light's sphere into all six cube faces
 * (wwCellShadowPass, and lampShadowPass's drawFace). A face's projection is 90 degrees about its major axis m, with
 * GL's s / t axes u / v: a point r (relative to the light) lands in the face only when m.r >= |u.r| and m.r >= |v.r|
 * (clip x = u.r, y = v.r, w = m.r). A sphere wholly outside one of those four planes puts no fragment into that face,
 * so skipping it there changes no depth texel: the pass is exact (the same depths, any draw order, LEQUAL).
 * The planes go through the light: n = m - u (and m + u, m - v, m + v), |n| = sqrt 2, so the sphere (c, rad) is
 * outside when n.c < -rad sqrt 2. `pad` adds 1 unit plus 1e-3 of the distance for the GPU's float transform.
 * The same table as celllights.cpp's face projection (tests/features/lights/cubefacecull checks the text there).
 * Red: WW_CELL_SHADOW_RED=allfaces draws every caster into every face again (celllights.cpp). */

#include <cmath>

namespace WwCubeFace
{

constexpr float kM[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
constexpr float kU[6][3] = { { 0, 0, -1 }, { 0, 0, 1 }, { 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
constexpr float kV[6][3] = { { 0, -1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, -1, 0 }, { 0, -1, 0 } };

inline float dot3( const float a[3], const float b[3] )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

//! the sphere (c relative to the light, radius rad) can put a fragment into face f
inline bool touches( int f, const float c[3], float rad )
{
	const float m = dot3( kM[f], c ), u = dot3( kU[f], c ), v = dot3( kV[f], c );
	const float len = std::sqrt( dot3( c, c ) );
#ifdef CUBEFACECULL_RED
	const float pad = 1e-3f * len + 1.0f;	// the red: the sphere's centre only (drops a caster that crosses the edge)
#else
	const float pad = rad * 1.41421356f + 1e-3f * ( len + rad ) + 1.0f;
#endif
	return m - u >= -pad && m + u >= -pad && m - v >= -pad && m + v >= -pad;
}

//! the faces (bit f) the sphere can reach
inline int mask( const float c[3], float rad )
{
	int b = 0;
	for ( int f = 0; f < 6; f++ )
		b |= touches( f, c, rad ) ? 1 << f : 0;
	return b;
}

/* lane PERFFIX2 b (item 5c): A SPOT LIGHT'S CONE. A spot lights nothing outside its cone: cell_lights.glsl's cone term
 * base = 1 - (1 - dot(-L, dir)) / (1 - cosOuter) is 0 there, and cellShadowF is only asked where the light is > 0. So a
 * face whose directions all lie outside the cone (widened by the lookup's reach: the 3x3 taps, their 2x2 compare, the
 * 1.5-texel normal lift) is never read for a lit pixel and needs no casters. The face's best direction toward the axis
 * d is found exactly: inside the face when d projects into it, else on one of its 4 edges (s = +-1 or t = +-1), where
 * (a + b t) / sqrt(2 + t^2) peaks at t = 2b / a (or an end). Debug probe 7 (WW_CELL_PROBE) still reads the faces off
 * the cone. Red WW_CELL_SHADOW_RED=nocone (or allfaces): every face drawn. */

//! the largest cosine between the axis d (unit) and any direction of face f
inline float faceMaxCos( int f, const float d[3] )
{
	const double m = dot3( kM[f], d ), u = dot3( kU[f], d ), v = dot3( kV[f], d );
	if ( m > 0.0 && std::fabs( u ) <= m && std::fabs( v ) <= m )
		return 1.0f;
	double best = -1.0;
	// the 4 edges: (m + s u + t v) / sqrt(1 + s^2 + t^2) with one of s, t at +-1
	for ( int e = 0; e < 4; e++ ) {
		const double sgn = ( e & 1 ) ? -1.0 : 1.0;
		const double a = e < 2 ? m + sgn * u : m + sgn * v;	// the fixed coordinate's share
		const double b = e < 2 ? v : u;			// the free one's
		double ts[3] = { -1.0, 1.0, 0.0 };
		int n = 2;
		if ( std::fabs( a ) > 1e-12 ) {
			const double t = 2.0 * b / a;
			if ( t > -1.0 && t < 1.0 )
				ts[n++] = t;
		}
		for ( int k = 0; k < n; k++ )
			best = std::fmax( best, ( a + b * ts[k] ) / std::sqrt( 2.0 + ts[k] * ts[k] ) );
	}
	return float( best );
}

//! the faces (bit f) a spot of axis dir (any length) and cone cos(half angle) = cosOuter can light, widened by `margin` rad
inline int coneMask( const float dir[3], float cosOuter, float margin )
{
	const float l = std::sqrt( dot3( dir, dir ) );
	if ( !( l > 1e-6f ) )
		return 63;
	const float d[3] = { dir[0] / l, dir[1] / l, dir[2] / l };
	const double half = std::acos( std::fmax( -1.0, std::fmin( 1.0, double( cosOuter ) ) ) ) + double( margin );
	if ( half >= 3.14159265358979 )
		return 63;
	const double cosW = std::cos( half );
	int b = 0;
	for ( int f = 0; f < 6; f++ ) {
#ifdef CUBEFACECULL_RED_CONE
		b |= dot3( kM[f], d ) >= cosW ? 1 << f : 0;	// the red: the face axis inside the cone only (drops a crossed face)
#else
		b |= double( faceMaxCos( f, d ) ) >= cosW - 1e-6 ? 1 << f : 0;
#endif
	}
	return b;
}

//! the lookup's reach past the cone, rad: 8 texels of a face of `face` texels (2 / face rad a texel) + 0.02
inline float coneMargin( int face )
{
	return 16.0f / float( face > 0 ? face : 256 ) + 0.02f;
}

}	// namespace WwCubeFace

#endif
