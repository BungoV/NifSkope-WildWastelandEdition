#ifndef FARSWAYSLICE_H
#define FARSWAYSLICE_H

/* lane SHADOWSHIMMER4 (FARSWAYCOST2, 2026-10-31): which far-ring tiles sway in a sun cascade.
 *
 * SHADOWSHIMMER3 made every far tree bucket a swaying caster, so SHADOWCACHE1's static cascade layers lost them: every
 * paint each cascade blitted its static layer and drew the whole bucket again (offline bench of Concord's rings 0-2
 * through the real caster programs, 5070 Ti: Ultra 1.43 ms median / 2.70 ms worst of 32 sun x camera views, High 1.18 /
 * 1.81, against SHADOWPAR1's whole shadow budget of 0.8 ms at 1080p).
 *
 * The sway is there for what the cascade SHADES: a far crown's self-shadow follows its own swaying crown
 * (SHADOWSHIMMER2's FARSWAYSHADOW). A cascade only shades the pixels of its slice of the view, so a far bucket's 4x4-cell
 * tile sways in cascade i only where its sphere meets the camera frustum between the slice's planar depths (blend band
 * included); the bucket's other tiles cast still and stay in the static layer, drawn only when the cache redraws. The
 * one look change: a still-casting tile's shadow on a receiver of another slice is at most the sway amplitude
 * (WW_CELL_FARLOD_SWAY, 10 units) from where the swaying tile would put it. Red WW_SUN_SHADOW_STABLE_RED=swayall:
 * every tile sways in every cascade again (SHADOWSHIMMER3's path). Gate: tests/shadowshimmer4. */

#include <cmath>

namespace WwFarSwaySlice
{

struct Slice
{
	double tanX = 1.0, tanY = 1.0;	// the camera frustum (symmetric, as gl/sunshadow.cpp reads it off the projection)
	double zn = 0.0, zf = 0.0;	// the cascade's slice, planar view depth in VIEW units (world units x the view scale)
	bool empty = false;		// the whole slice lies inside the loaded block, where no far vertex sways (reachInside)
};

/*! The slice holds no swaying far vertex: every point of the view slice is within zf sqrt(1 + tanX^2 + tanY^2) of the
 *  eye, and lookdev_csmdepth(_at).vert sways only vertices outside the loaded block's square (farSwayBlock.w); `edge` is
 *  the eye's distance to that square's border (half - the Chebyshev offset from its centre; <= 0 outside it). World units. */
inline bool reachInside( double zf, double tanX, double tanY, double edge )
{
	return edge > 0.0 && zf * std::sqrt( 1.0 + tanX * tanX + tanY * tanY ) < edge;
}

//! A tile sphere (centre in view space, the camera looking down -z; radius in view units) meets the view slice.
inline bool tileMeets( const Slice & s, double x, double y, double z, double r )
{
	if ( s.empty )
		return false;
	const double d = -z;
	if ( d < s.zn - r || d > s.zf + r )
		return false;
	const double kx = std::sqrt( 1.0 + s.tanX * s.tanX ), ky = std::sqrt( 1.0 + s.tanY * s.tanY );
	if ( ( s.tanX * d - x ) / kx < -r || ( s.tanX * d + x ) / kx < -r )
		return false;
	return ( s.tanY * d - y ) / ky >= -r && ( s.tanY * d + y ) / ky >= -r;
}

enum Part { None = 0, Still = 1, Sway = 2, Both = 3 };

/*! The tiles of a split far caster one cascade job draws. Set 0 (no cache): both parts, every pass. Set 1 (the static
 *  layer): the still tiles, unless the caster is dynamic anyway (changed lately: then the static layer lacks it whole).
 *  Set 2 (over the live layer): the swaying tiles, and the still ones too when the caster is dynamic. Every tile is
 *  then drawn exactly once a pass, still or swaying as set 0 draws it. */
inline int parts( int set, bool dynamic )
{
	return set == 0 ? Both : set == 1 ? ( dynamic ? None : Still ) : ( dynamic ? Both : Sway );
}

} // namespace WwFarSwaySlice

#endif
