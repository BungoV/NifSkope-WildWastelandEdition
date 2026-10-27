#ifndef WW_WATERPLANEFIT_H
#define WW_WATERPLANEFIT_H

/* Lane WATERUNDER5: the placed-water fallback of the under-water test (no baked body at the camera), pure maths so
 * tests/waterplane checks the same code the cell view runs.
 *  - covers: (x, y) lies in one of the shape's triangles (its polygon), not only in its XY bounding box;
 *  - height: the shape's least-squares mean plane at (x, y); a shape that is not one plane (residual > kFlatTol,
 *    e.g. one bucket of cell water quads at two heights) reads the covering triangle's own plane instead;
 *  - room: a non-water triangle crossing the vertical segment from the camera up to the surface means the camera is
 *    in a room below the water (a basement under a pond), not in it.
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

#endif
