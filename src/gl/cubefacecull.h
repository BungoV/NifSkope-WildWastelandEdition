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
 * The same table as celllights.cpp's face projection (tests/cubefacecull checks the text there).
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

}	// namespace WwCubeFace

#endif
