#ifndef WW_AIRLAMPMOVER_H
#define WW_AIRLAMPMOVER_H

/* lane AIRPERF1 A5: when does a moved caster redraw a lamp's shadow tiles? (std only; the unit:
 * notes/modules1/airperf1/unit/lampmover.cpp). Before, any changed bit of a caster's signature redrew every lamp it
 * touched, every frame: a shape that breathes or sways by a fraction of a unit kept its lamps redrawing for good.
 * Now the move is measured from the pose the tiles were last drawn for (the reference), as an upper bound of how far
 * any point of the caster went, and a lamp is redrawn when that bound reaches half a texel of its tile at the
 * caster's nearest point (below that the drawn shadow cannot differ by more than the filter hides). The reference
 * stays until a redraw, so a slow creep adds up and is drawn. */

#include <algorithm>
#include <cmath>

namespace airmover
{

constexpr float kNoise = 1.0e-3f;	// game units: under this nothing is redrawn (float noise of a rebuilt transform)

struct Parts
{
	float centre = 0.0f, turn = 0.0f, scale = 0.0f, verts = 0.0f, radius = 0.0f;
	float sum() const { return centre + turn + scale + verts + radius; }
};

inline float dist3( const float a[3], const float b[3] )
{
	const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
	return std::sqrt( x * x + y * y + z * z );
}

/*! An upper bound of the displacement of any point of the caster between two signatures. v: rotation 9, translation
 * 3, scale, first vertex 3, middle vertex 3 (airlampatlas.cpp sigOf); c, rad: its world sphere. A point at offset o
 * from the centre (|o| <= rad) moves by at most |dc| + |dR| rad + |ds| / s rad; a CPU-skinned shape's vertices move
 * in local space: twice the larger of the two sampled vertices' moves (a sample, not a bound: the margin of 2). */
inline Parts bound( const float va[19], const float ca[3], float ra, const float vb[19], const float cb[3], float rb )
{
	Parts p;
	p.centre = dist3( ca, cb );
	float r2 = 0.0f;
	for ( int i = 0; i < 9; i++ )
		r2 += ( va[i] - vb[i] ) * ( va[i] - vb[i] );
	const float rad = std::max( ra, rb ), s = std::max( std::fabs( va[12] ), std::fabs( vb[12] ) );
	p.turn = std::sqrt( r2 ) * rad;
	p.scale = s > 0.0f ? std::fabs( va[12] - vb[12] ) / s * rad : 0.0f;
	p.verts = 2.0f * s * std::max( dist3( va + 13, vb + 13 ), dist3( va + 16, vb + 16 ) );
	p.radius = std::fabs( ra - rb );
	return p;
}

/*! The move under which a lamp's tile cannot show the difference: half a texel at the caster's nearest point. A cube
 * face spans 2 at distance 1 over tilePx texels, so half a texel at distance D is D / tilePx. */
inline float threshold( const float lamp[3], const float c[3], float rad, int tilePx )
{
	const float D = std::max( dist3( lamp, c ) - rad, 0.0f );
	return std::max( D / float( std::max( tilePx, 1 ) ), kNoise );
}

//! a changed caster redraws its lamps: exact = the rule before (any changed bit)
inline bool redraws( float moved, float thr, bool exact )
{
#ifdef AIRMOVER_RED_EXACT
	exact = true;
#endif
	return exact || moved >= thr;
}

/*! One changed frame of a caster. touched: the lamps with tiles whose reach holds it at the reference or now; thrMin:
 * the smallest threshold of those; crossed: it entered or left a lamp's reach (that lamp's tiles gain or lose it). */
inline bool decide( float moved, float thrMin, int touched, bool crossed, bool exact )
{
	return touched > 0 && ( crossed || redraws( moved, thrMin, exact ) );
}

}	// namespace airmover

#endif
