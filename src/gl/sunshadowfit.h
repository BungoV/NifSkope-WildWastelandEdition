/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_SUNSHADOWFIT_H
#define WW_SUNSHADOWFIT_H

/* lane SHADOWSHIMMER1 (bungo 10-29: "shadows move for some reason, yet they shouldn't, the leaves are totally static"):
 * the cascade fit and the receiver kernel's rotation, pure C++ so tests/shadowshimmer1 runs the very code
 * gl/sunshadow.cpp runs (no Qt, no GL).
 *
 * fitOld: CSM1's fit (spec 2.1-2.3) as it shipped: the slice's corners boxed in light space, texel = ceil(extent /
 *   map) (or 1/n), the light origin and the box corner each floored onto the texel. A camera TURN changes the box, so
 *   the texel and the window jump: every shadow edge crawls. Red WW_SUN_SHADOW_STABLE_RED=fit (or old).
 * fitStable (the default): the slice's bounding SPHERE (rotation-invariant radius; the first slice starts at the eye,
 *   never at the near plane, which follows the scene), texel = diameter / (map - 1), the window's low corner floored
 *   onto whole texels in ABSOLUTE light space (a slide moves it by whole texels only), the depth range a fixed bound
 *   (lightBack + centre + radius, plus 64 depth quanta of margin) and its origin floored onto 64-quantum steps (a move
 *   along the light never re-quantises a stored depth; the cache keeps scrolling between steps).
 *
 * The kernel (res/shaders/ww_sunshadow.glsl ussVogel): the Vogel disc's rotation was 2 pi IGN(gl_FragCoord + frame %
 *   61 x (32.665, 11.815)): a different rotation every frame under TAA and a screen-locked one that swims over the
 *   world as the camera moves. Stable: 2 pi IGN of the absolute light-space lattice cell (kCells per shadow texel,
 *   wrapped at kCellWrap), the same for a still world point in every frame and from every camera.
 *   rotOld / rotStable below mirror the GLSL line for line (the unit checks the shader's text for the constants). */

#include <algorithm>
#include <cmath>

namespace WwCsmFit
{

constexpr double kNear = 150.0;		// the light's near plane (spec 2.3)
constexpr int kCells = 4;		// stable kernel: lattice cells per shadow texel (ww_sunshadow.glsl "* 4.0")
constexpr double kCellWrap = 4096.0;	// the lattice index wraps here (IGN's float input stays small)
constexpr int kDepthStep = 64;		// stable fit: the depth origin moves in steps of this many D16 quanta

struct In
{
	double C[3] = { 0, 0, 0 }, fwd[3] = { 0, 0, -1 };	// the camera, world
	double rc[3] = { 1, 0, 0 }, uc[3] = { 0, 1, 0 };	// its right and up rows
	double tanX = 1, tanY = 1;
	double R[3] = { 1, 0, 0 }, U[3] = { 0, 1, 0 }, L[3] = { 0, 0, -1 };	// the light basis, L = travel direction
	double zn = 0, zf = 0;		// the slice, planar view depth (world units)
	bool first = false;		// the first slice
	int map = 2048;
	double lightBack = 15000.0;
	bool nosnap = false;		// CSM1's red WW_CSM_RED=nosnap
};

struct Out
{
	double minX = 0, maxX = 0, minY = 0, maxY = 0, maxZ = 0;	// light-space box relative to the camera (z + lightBack)
	double w = 0, h = 0;		// extents
	double texel = 1;
	int n = 0;			// the old sub-unit branch: texel = 1/n (0 = integer branch / stable)
	int vw = 0, vh = 0;		// viewport, texels
	double pb[3] = { 0, 0, 0 };	// the snapped light origin
	double l = 0, b = 0;		// the snapped window corner (relative to pb; 0 in the stable fit)
	double far = 0;
	double sphereZ = 0, sphereR = 0;	// stable: the sphere's centre along fwd and its radius (0 = the old box)
};

inline double dot3( const double * a, const double * b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

//! the spec's snap: onto the texel (integer branch) or onto 1/n (the sub-unit branch)
inline double snapOld( double v, const Out & c, bool nosnap )
{
	if ( nosnap )
		return v;
	if ( c.n > 0 )
		return std::floor( v * c.n ) / c.n;
	return std::floor( v / c.texel ) * c.texel;
}

//! CSM1's fit, as gl/sunshadow.cpp ran it before SHADOWSHIMMER1 (the red)
inline void fitOld( const In & in, Out & c )
{
	c = Out();
	bool first = true;
	for ( int zi = 0; zi < 2; zi++ ) {
		const double z = zi ? in.zf : in.zn;
		for ( int sx = -1; sx <= 1; sx += 2 ) {
			for ( int sy = -1; sy <= 1; sy += 2 ) {
				double q[3];	// Q - C
				for ( int j = 0; j < 3; j++ )
					q[j] = z * ( in.fwd[j] + sx * in.tanX * in.rc[j] + sy * in.tanY * in.uc[j] );
				const double x = dot3( q, in.R ), y = dot3( q, in.U ), zz = dot3( q, in.L ) + in.lightBack;
				if ( first ) {
					c.minX = c.maxX = x;
					c.minY = c.maxY = y;
					c.maxZ = zz;
					first = false;
				} else {
					c.minX = std::min( c.minX, x );
					c.maxX = std::max( c.maxX, x );
					c.minY = std::min( c.minY, y );
					c.maxY = std::max( c.maxY, y );
					c.maxZ = std::max( c.maxZ, zz );
				}
			}
		}
	}
	c.w = std::floor( c.maxX - c.minX );
	c.h = std::floor( c.maxY - c.minY );
	const double ext = std::max( std::max( c.w, c.h ), 1.0 );
	const double ratio = ext / in.map;
	if ( in.nosnap ) {
		c.texel = ratio;
		c.n = 0;
	} else if ( ratio > 0.5 ) {
		c.texel = std::ceil( ratio );
		c.n = 0;
	} else {
		c.n = std::max( 1, int( std::floor( 1.0 / ratio ) ) );
		c.texel = 1.0 / c.n;
	}
	c.pb[0] = snapOld( dot3( in.C, in.R ), c, in.nosnap );
	c.pb[1] = snapOld( dot3( in.C, in.U ), c, in.nosnap );
	c.pb[2] = snapOld( dot3( in.C, in.L ) - in.lightBack, c, in.nosnap );
	c.l = snapOld( c.minX, c, in.nosnap );
	c.b = snapOld( c.minY, c, in.nosnap );
	c.vw = std::clamp( int( std::floor( c.w / c.texel + 1e-9 ) ), 1, in.map );
	c.vh = std::clamp( int( std::floor( c.h / c.texel + 1e-9 ) ), 1, in.map );
	c.far = c.maxZ + kNear;
}

//! the slice's bounding sphere: centre distance along fwd and radius (rounded up to a whole unit)
inline void sliceSphere( const In & in, double & zc, double & r )
{
	const double k = std::sqrt( in.tanX * in.tanX + in.tanY * in.tanY );
	const double n0 = in.first ? 0.0 : in.zn, f0 = in.zf;
	// equidistant from the near and far corner rings: (z - n)^2 + (n k)^2 = (f - z)^2 + (f k)^2, never past the far plane
	zc = std::min( 0.5 * ( f0 + n0 ) * ( 1.0 + k * k ), f0 );
	r = std::ceil( std::max( std::hypot( zc - n0, n0 * k ), std::hypot( f0 - zc, f0 * k ) ) );
}

//! SHADOWSHIMMER1's fit: the sphere, whole texels in absolute light space, a fixed depth range on 64-quantum steps
inline void fitStable( const In & in, Out & c )
{
	c = Out();
	double zc = 0, r = 0;
	sliceSphere( in, zc, r );
	double ctr[3];
	for ( int j = 0; j < 3; j++ )
		ctr[j] = in.C[j] + zc * in.fwd[j];
	const double ext = 2.0 * r;
	c.texel = ext / double( std::max( in.map - 1, 1 ) );
	c.n = 0;
	const double x0 = dot3( ctr, in.R ) - r, y0 = dot3( ctr, in.U ) - r;
	c.pb[0] = in.nosnap ? x0 : std::floor( x0 / c.texel ) * c.texel;
	c.pb[1] = in.nosnap ? y0 : std::floor( y0 / c.texel ) * c.texel;
	c.l = 0.0;
	c.b = 0.0;
	c.vw = c.vh = std::clamp( int( std::ceil( ext / c.texel - 1e-9 ) ) + 1, 1, in.map );
	c.w = c.h = c.vw * c.texel;
	c.minX = c.pb[0] - dot3( in.C, in.R );
	c.maxX = c.minX + c.w;
	c.minY = c.pb[1] - dot3( in.C, in.U );
	c.maxY = c.minY + c.h;
	// depth: wz = 65535 quanta, kDepthStep of them margin over the bound lightBack + zc + r (any turn, any slide)
	const double wz0 = in.lightBack + zc + r;
	const double dq = wz0 / double( 65535 - kDepthStep );
	c.maxZ = 65535.0 * dq;
	c.far = c.maxZ + kNear;
	const double step = kDepthStep * dq, z0 = dot3( in.C, in.L ) - in.lightBack;
	c.pb[2] = in.nosnap ? z0 : std::floor( z0 / step ) * step;
	c.sphereZ = zc;
	c.sphereR = r;
}

// ---- the receiver kernel's rotation (ww_sunshadow.glsl ussIgn / ussVogel) ----

inline double ign( double x, double y )
{
	const double a = x * 0.06711056 + y * 0.00583715;
	const double f = 52.9829189 * ( a - std::floor( a ) );
	return f - std::floor( f );
}

//! GLSL mod(x, y) (never negative for y > 0)
inline double glslMod( double x, double y )
{
	return x - y * std::floor( x / y );
}

//! the window's low corner in lattice cells, wrapped (gl/sunshadow.cpp uploads it as ussCsmCellX/Y[i])
inline void cellOrigin( const Out & c, double & ox, double & oy )
{
	ox = glslMod( std::round( ( c.pb[0] + c.l ) / c.texel ) * kCells, kCellWrap );
	oy = glslMod( std::round( ( c.pb[1] + c.b ) / c.texel ) * kCells, kCellWrap );
}

//! the old rotation: the pixel the point lands in, and the TAA frame's phase (frame % 61, 0 without TAA)
inline double rotOld( double fragX, double fragY, double phase )
{
	return 6.2831853 * ign( fragX + phase * 32.665, fragY + phase * 11.815 );
}

//! the stable rotation of world point Q in cascade window c (light basis R, U)
inline double rotStable( const Out & c, const double R[3], const double U[3], const double Q[3] )
{
	double ox, oy;
	cellOrigin( c, ox, oy );
	const double u = ( dot3( Q, R ) - ( c.pb[0] + c.l ) ) / c.texel, v = ( dot3( Q, U ) - ( c.pb[1] + c.b ) ) / c.texel;	// q.xy x map
	const double x = glslMod( std::floor( u * kCells ) + ox, kCellWrap ), y = glslMod( std::floor( v * kCells ) + oy, kCellWrap );
	return 6.2831853 * ign( x, y );
}

} // namespace WwCsmFit

#endif
