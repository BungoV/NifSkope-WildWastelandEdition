#ifndef CELLDECALANGLE_H
#define CELLDECALANGLE_H

/* lane DECALANGLE (2026-10-26): the placed decal's ANGLE RULE, the game's way (per pixel).
 *
 * With D = minus the projection axis: a pixel whose drawn face (the flat triangle) has dot(G, D) >= 0.3 takes the
 * decal whole; otherwise it takes saturate((dot(normalize(N), D) - 0.3) / 0.25), N the surface's interpolated
 * shading normal at that pixel; texture alpha x that term under 4/255 is dropped.
 *
 * The cell view clips the decal pieces on the CPU, so the per-pixel half rides the vertex alpha: each piece vertex
 * carries dot(n, D) of its own unit normal (clamped to 0..1), or 1 when its face passes, and fo4_default.frag
 * (wwDecalVertexA) divides the interpolated value by the interpolated normal's length -- which is dot(normalize(N), D)
 * exactly -- before the fade. The old rule (red "vertexangle") faded per vertex and interpolated the faded value.
 * No Qt, no GL: tests/decalangle_gate.cpp includes this file. */

#include <algorithm>
#include <cmath>

namespace celldecalangle {

constexpr float kMin = 0.3f;              // the game's threshold (both tests)
constexpr float kFade = 0.25f;            // the fade width
constexpr float kDrop = 4.0f / 255.0f;    // texture alpha x term under this is discarded

inline float sat( float x )
{
	return x < 0.0f ? 0.0f : ( x > 1.0f ? 1.0f : x );
}

//! The game's term at one pixel: the face's dot and the interpolated (not unit) shading normal's dot and length.
inline float gameTerm( float faceDot, float nDot, float nLen )
{
	if ( faceDot >= kMin )
		return 1.0f;
	return sat( ( nDot / std::max( nLen, 1.0e-6f ) - kMin ) / kFade );
}

//! The value a piece vertex carries in its alpha (per pixel rule). unitDot = dot(unit vertex normal, D).
inline float vertexValue( float faceDot, float unitDot )
{
	return faceDot >= kMin ? 1.0f : sat( unitDot );
}

//! The fragment side (mirrors wwDecalVertexA in res/shaders/fo4_default.frag): interpolated value and normal length.
inline float fragTerm( float value, float nLen )
{
	return sat( ( value / std::max( nLen, 1.0e-4f ) - kMin ) / kFade );
}

//! Red "vertexangle" (the rule before this lane): fade per vertex, interpolated by the rasteriser.
inline float vertexFade( float faceDot, float unitDot )
{
	return faceDot >= kMin ? 1.0f : sat( ( unitDot - kMin ) / kFade );
}

/* The CPU pre-cull for a face under the threshold: can any pixel of it pass? Inside the triangle the interpolated
 * normal is a convex mix of the three unit normals n0..n2, so its dot with D is at most dMax and its length is at
 * least the distance from the origin to the plane through n0..n2 (the triangle of tips lies in that plane). A
 * pixel passes only if dot / length > 0.3, so dMax <= 0.3 x that distance rejects safely. */
inline bool mayPass( float dMax, const float n0[3], const float n1[3], const float n2[3] )
{
	if ( dMax <= 0.0f )
		return false;
	const float a[3] = { n1[0] - n0[0], n1[1] - n0[1], n1[2] - n0[2] };
	const float b[3] = { n2[0] - n0[0], n2[1] - n0[1], n2[2] - n0[2] };
	const float c[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
	const float cl = std::sqrt( c[0] * c[0] + c[1] * c[1] + c[2] * c[2] );
	float lenLow = 1.0f;
	if ( cl > 1.0e-6f )
		lenLow = std::fabs( n0[0] * c[0] + n0[1] * c[1] + n0[2] * c[2] ) / cl;
	else {   // the tips lie on a line (or a point): the nearest point of the segment bounds the length
		float lo = 1.0f;
		const float * e[3][2] = { { n0, n1 }, { n1, n2 }, { n0, n2 } };
		for ( auto & s : e ) {
			const float d[3] = { s[1][0] - s[0][0], s[1][1] - s[0][1], s[1][2] - s[0][2] };
			const float dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
			float t = dd > 1.0e-12f ? -( s[0][0] * d[0] + s[0][1] * d[1] + s[0][2] * d[2] ) / dd : 0.0f;
			t = sat( t );
			const float p[3] = { s[0][0] + d[0] * t, s[0][1] + d[1] * t, s[0][2] + d[2] * t };
			lo = std::min( lo, std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] ) );
		}
		lenLow = lo;
	}
	return dMax > kMin * std::min( lenLow, 1.0f ) * 0.999f;
}

/* lane DECALANGLE2: the DRAWN normal (the receiver's normal map), as the game reads its G-buffer. The decal pass reads
 * the cell view's shared opaque pass (view normal per pixel, src/gl/celllights.cpp) and needs D itself, carried in
 * the vertex colour's rgb as a signed byte: byte = round(127 d + 128), d = (byte - 128) / 127 (0 and +-1 exact). */
inline float dirToColour( float d )
{
	return std::round( 127.0f * std::max( -1.0f, std::min( 1.0f, d ) ) + 128.0f ) / 255.0f;
}

inline float dirFromColour( float c )
{
	return ( c * 255.0f - 128.0f ) / 127.0f;
}

//! The face passes when every corner carries 1 (vertexValue): the interpolated value then stays 1.
constexpr float kFacePass = 254.5f / 255.0f;

//! The term on the drawn normal: value = the interpolated vertex value, gDot = dot(drawn unit normal, D).
inline float mappedTerm( float value, float gDot )
{
	return value >= kFacePass ? 1.0f : sat( ( gDot - kMin ) / kFade );
}

/* A normal map leans the drawn normal off the vertex normal; up to kLean (45 degrees) a pixel can pass only while the
 * interpolated vertex normal's angle to D is under acos(0.3) + 45 deg, i.e. its dot over -0.46. The interpolated
 * normal's normalised dot is at most dMax / length <= dMax when dMax < 0, so dMax <= kLeanDot rejects safely. */
constexpr float kLeanDot = -0.4625f;   // just under cos(acos(0.3) + 45 deg) = -0.4624

inline bool mayPassMapped( float dMax )
{
	return dMax > kLeanDot;
}

}	// namespace celldecalangle

#endif
