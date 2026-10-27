#ifndef WW_WATERPLANEFIT_H
#define WW_WATERPLANEFIT_H

/* Lane WATERUNDER5: the placed-water fallback of the under-water test (no baked body at the camera), pure maths so
 * tests/waterplane checks the same code the cell view runs.
 *  - covers: (x, y) lies in one of the shape's triangles (its polygon), not only in its XY bounding box;
 *  - height: the shape's least-squares mean plane at (x, y); a shape that is not one plane (residual > kFlatTol,
 *    e.g. one bucket of cell water quads at two heights) reads the covering triangle's own plane instead;
 *  - room: a non-water triangle crossing the vertical segment from the camera up to the surface means the camera is
 *    in a room below the water (a basement under a pond), not in it, if the room is also closed (wwWaterRoomOver).
 * oldRule = WATERUNDER1's rule (bounding box, highest vertex, no room test): the red. */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

struct WwWaterPoly
{
	std::vector<float> tri;	// world triangles, 9 floats each
	float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f, top = -1e30f;
	double a = 0.0, b = 0.0, c = 0.0;	// mean plane z = a x + b y + c
	double resid = 0.0;	// largest |z - plane| over the vertices
	int body = 0;
};

constexpr double kWwWaterFlatTol = 2.0;	// game units: a shape within this of its mean plane is one plane

//! the shape from its world triangles (9 floats each); empty input gives an empty poly (covers nothing)
inline WwWaterPoly wwWaterPolyFrom( const std::vector<float> & tri, int body )
{
	WwWaterPoly p;
	p.tri = tri;
	p.body = body;
	const size_t n = tri.size() / 3;
	if ( !n )
		return p;
	double mx = 0, my = 0, mz = 0;
	for ( size_t i = 0; i < n; i++ ) {
		const float x = tri[3 * i], y = tri[3 * i + 1], z = tri[3 * i + 2];
		p.x0 = std::min( p.x0, x );
		p.y0 = std::min( p.y0, y );
		p.x1 = std::max( p.x1, x );
		p.y1 = std::max( p.y1, y );
		p.top = std::max( p.top, z );
		mx += x;
		my += y;
		mz += z;
	}
	mx /= double( n );
	my /= double( n );
	mz /= double( n );
	// centred normal equations of z - mz = a (x - mx) + b (y - my)
	double sxx = 0, sxy = 0, syy = 0, sxz = 0, syz = 0;
	for ( size_t i = 0; i < n; i++ ) {
		const double dx = tri[3 * i] - mx, dy = tri[3 * i + 1] - my, dz = tri[3 * i + 2] - mz;
		sxx += dx * dx;
		sxy += dx * dy;
		syy += dy * dy;
		sxz += dx * dz;
		syz += dy * dz;
	}
	const double det = sxx * syy - sxy * sxy;
	if ( std::fabs( det ) > 1e-9 * std::max( 1.0, sxx * syy ) ) {
		p.a = ( sxz * syy - syz * sxy ) / det;
		p.b = ( syz * sxx - sxz * sxy ) / det;
	}
	p.c = mz - p.a * mx - p.b * my;
	for ( size_t i = 0; i < n; i++ )
		p.resid = std::max( p.resid, std::fabs( tri[3 * i + 2] - ( p.a * tri[3 * i] + p.b * tri[3 * i + 1] + p.c ) ) );
	return p;
}

//! barycentric (u, v) of (x, y) in the XY projection of triangle t (9 floats); false if outside or degenerate
inline bool wwTriXY( const float * t, float x, float y, double & u, double & v )
{
	const double ax = t[0], ay = t[1];
	const double e1x = t[3] - ax, e1y = t[4] - ay, e2x = t[6] - ax, e2y = t[7] - ay;
	const double d = e1x * e2y - e1y * e2x;
	if ( std::fabs( d ) < 1e-12 )
		return false;
	const double px = x - ax, py = y - ay;
	u = ( px * e2y - py * e2x ) / d;
	v = ( e1x * py - e1y * px ) / d;
	const double eps = -1e-6;
	return u >= eps && v >= eps && u + v <= 1.0 - eps;
}

//! the surface height at (x, y) when the shape covers it (oldRule: the bounding box and the highest vertex)
inline bool wwWaterPolyAt( const WwWaterPoly & p, float x, float y, bool oldRule, float & h )
{
	if ( p.tri.empty() || x < p.x0 || x > p.x1 || y < p.y0 || y > p.y1 )
		return false;
	if ( oldRule ) {
		h = p.top;
		return true;
	}
	for ( size_t i = 0; i + 9 <= p.tri.size(); i += 9 ) {
		double u, v;
		if ( !wwTriXY( &p.tri[i], x, y, u, v ) )
			continue;
		if ( p.resid <= kWwWaterFlatTol ) {
			h = float( p.a * x + p.b * y + p.c );
		} else {
			const float * t = &p.tri[i];
			h = float( t[2] + u * ( t[5] - t[2] ) + v * ( t[8] - t[2] ) );
		}
		return true;
	}
	return false;
}

//! the lowest shape over (x, y, z) at most maxDepth above it: its index (or -1) and height
inline int wwWaterPolyPick( const std::vector<WwWaterPoly> & ps, float x, float y, float z, float maxDepth,
	bool oldRule, float & height )
{
	int best = -1;
	for ( size_t k = 0; k < ps.size(); k++ ) {
		float h;
		if ( !wwWaterPolyAt( ps[k], x, y, oldRule, h ) || !( h > z ) || h - z > maxDepth )
			continue;
		if ( best < 0 || h < height ) {
			best = int( k );
			height = h;
		}
	}
	return best;
}

//! true when a triangle of `tri` (9 floats each) crosses the vertical segment x, y, z0 < z < z1
inline bool wwWaterRoofOver( const float * tri, size_t nTri, float x, float y, float z0, float z1 )
{
	for ( size_t i = 0; i < nTri; i++ ) {
		const float * t = tri + 9 * i;
		double u, v;
		if ( !wwTriXY( t, x, y, u, v ) )
			continue;
		const double z = t[2] + u * ( t[5] - t[2] ) + v * ( t[8] - t[2] );
		if ( z > z0 && z < z1 )
			return true;
	}
	return false;
}

/* Lane WATERUNDER6: a roof alone is not a room. A sunken hull or a pier's underside over the camera stops the vertical
 * segment too, but water reaches the camera round its edges. A room is closed: besides the roof over the camera, every
 * ray of a cone (8 bearings at 45 and 15 degrees up) must meet a non-water surface before it rises to the water
 * surface or runs kWwRoomReach sideways. One open ray = the camera is in the water. */
constexpr double kWwRoomReach = 2048.0;	// game units sideways; a ray that runs this far unblocked is open

//! the ray o + t d crosses triangle t (9 floats, either face) at t = tHit
inline bool wwRayTri( const float * t, const double o[3], const double d[3], double & tHit )
{
	const double e1[3] = { double( t[3] ) - t[0], double( t[4] ) - t[1], double( t[5] ) - t[2] };
	const double e2[3] = { double( t[6] ) - t[0], double( t[7] ) - t[1], double( t[8] ) - t[2] };
	const double p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
	const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
	const double scale = std::sqrt( ( e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2] ) * ( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] ) );
	if ( !( std::fabs( det ) > 1e-12 * scale ) )
		return false;
	const double inv = 1.0 / det;
	const double s[3] = { o[0] - t[0], o[1] - t[1], o[2] - t[2] };
	const double u = ( s[0] * p[0] + s[1] * p[1] + s[2] * p[2] ) * inv;
	if ( u < -1e-9 || u > 1.0 + 1e-9 )
		return false;
	const double q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
	const double v = ( d[0] * q[0] + d[1] * q[1] + d[2] * q[2] ) * inv;
	if ( v < -1e-9 || u + v > 1.0 + 1e-9 )
		return false;
	tHit = ( e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2] ) * inv;
	return true;
}

//! the horizontal reach of the cone for a camera `depth` below the surface (the candidate shapes' XY margin)
inline double wwWaterRoomReach( double depth )
{
	return std::min( kWwRoomReach, depth / std::tan( 15.0 * 3.14159265358979 / 180.0 ) );
}

//! true when the camera (x, y, z) under a surface at h is in a closed room: a roof over it and every cone ray blocked
inline bool wwWaterRoomOver( const float * tri, size_t nTri, float x, float y, float z, float h )
{
	if ( !wwWaterRoofOver( tri, nTri, x, y, z, h ) )
		return false;
	// only triangles inside the cone's cylinder (z .. h, reach sideways) can block a ray
	const double reach = wwWaterRoomReach( double( h ) - z );
	std::vector<float> nearT;
	for ( size_t i = 0; i < nTri; i++ ) {
		const float * t = tri + 9 * i;
		const float zl = std::min( { t[2], t[5], t[8] } ), zh = std::max( { t[2], t[5], t[8] } );
		const float xl = std::min( { t[0], t[3], t[6] } ), xh = std::max( { t[0], t[3], t[6] } );
		const float yl = std::min( { t[1], t[4], t[7] } ), yh = std::max( { t[1], t[4], t[7] } );
		if ( zh < z || zl > h || xh < x - reach || xl > x + reach || yh < y - reach || yl > y + reach )
			continue;
		nearT.insert( nearT.end(), t, t + 9 );
	}
	const size_t nNear = nearT.size() / 9;
	const double o[3] = { x, y, z };
	const double kPi = 3.14159265358979;
	for ( const double elevDeg : { 15.0, 45.0 } ) {	// the low ring first: it escapes a hull or a pier soonest
		const double el = elevDeg * kPi / 180.0, ce = std::cos( el ), se = std::sin( el );
		const double tMax = std::min( ( double( h ) - z ) / se, kWwRoomReach / ce );
		for ( int a = 0; a < 8; a++ ) {
			const double az = a * kPi / 4.0;
			const double d[3] = { ce * std::cos( az ), ce * std::sin( az ), se };
			bool blocked = false;
			for ( size_t i = 0; i < nNear && !blocked; i++ ) {
				double t;
				blocked = wwRayTri( &nearT[9 * i], o, d, t ) && t > 1e-3 && t < tMax;
			}
			if ( !blocked )
				return false;
		}
	}
	return true;
}

#endif
