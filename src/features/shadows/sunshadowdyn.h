#ifndef WW_SUNSHADOWDYN_H
#define WW_SUNSHADOWDYN_H

/* lane PERFFIX2 b (item 8a, 11-01): THE SHADOW CACHE'S HIT RESTORE, CUT TO WHAT THE DYNAMIC CASTERS TOUCHED.
 *
 * A Hit cascade (gl/sunshadow.cpp ShadowCache) copied its whole static layer into the live map (map x map D16, 0.12 ms
 * at 4096) before drawing the dynamic casters over it, every paint. The live map already IS the static layer except
 * where last paint's dynamic casters wrote. So, when the static layer is the same one the live map was copied from
 * (same key, no Full / Scroll since), only last paint's dynamic footprint is copied back; the new dynamic casters are
 * drawn over it and their footprint kept for the next paint. Depth-only LEQUAL: the live map ends texel for texel what
 * the full copy + the same draws give (tests/features/shadows/sunshadowdyn models both over moving casters).
 * The footprint: each dynamic caster's view-space box (its drawn vertices: skinned, cable sway or its own; a sway
 * caster's box grown by the shader's largest offset; a far tile by its sphere) through the cascade's ORTHOGRAPHIC
 * clip rows (x and y are affine, so the 8 corners bound it), to texels, floor / ceil, 1 texel more, clamped to the
 * viewport. A caster with no known footprint makes the cascade copy the whole layer, as before.
 * Red WW_SHADOWCACHE_RED=fullcopy: the whole copy every Hit paint (the old cost; pictures equal). */

#include <algorithm>
#include <cmath>

namespace WwSunDyn
{

//! a texel rect [x0, x1) x [y0, y1); empty when x0 >= x1 or y0 >= y1
struct Rect
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	bool empty() const { return x0 >= x1 || y0 >= y1; }
};

//! an ndc-space box (x0, y0, x1, y1), open: nothing yet
struct Ndc
{
	float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
	bool any() const { return x0 <= x1 && y0 <= y1; }
	void add( float x, float y )
	{
		x0 = std::min( x0, x );
		x1 = std::max( x1, x );
		y0 = std::min( y0, y );
		y1 = std::max( y1, y );
	}
};

//! the view-space box [lo, hi] grown by `grow` view units, through the column-major clip rows m (x and y affine)
inline void addBox( Ndc & n, const float m[16], const float lo[3], const float hi[3], float grow )
{
	for ( int k = 0; k < 8; k++ ) {
		const float p[3] = { ( k & 1 ) ? hi[0] + grow : lo[0] - grow, ( k & 2 ) ? hi[1] + grow : lo[1] - grow,
			( k & 4 ) ? hi[2] + grow : lo[2] - grow };
		n.add( m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13] );
	}
}

//! a view-space sphere through the same rows (its x / y reach per axis)
inline void addSphere( Ndc & n, const float m[16], const float c[3], float r )
{
	const float x = m[0] * c[0] + m[4] * c[1] + m[8] * c[2] + m[12];
	const float y = m[1] * c[0] + m[5] * c[1] + m[9] * c[2] + m[13];
	const float rx = r * std::sqrt( m[0] * m[0] + m[4] * m[4] + m[8] * m[8] );
	const float ry = r * std::sqrt( m[1] * m[1] + m[5] * m[5] + m[9] * m[9] );
	n.add( x - rx, y - ry );
	n.add( x + rx, y + ry );
}

//! the ndc box in the (vw, vh) viewport's texels: floor / ceil, `margin` texels more, clamped
inline Rect texels( const Ndc & n, int vw, int vh, int margin = 1 )
{
	Rect r;
	if ( !n.any() )
		return r;
#ifdef SUNDYN_RED_TRUNC
	// the red: truncates both edges with no margin (drops the covered texels past the last whole one)
	r.x0 = int( ( n.x0 * 0.5 + 0.5 ) * vw );
	r.x1 = int( ( n.x1 * 0.5 + 0.5 ) * vw );
	r.y0 = int( ( n.y0 * 0.5 + 0.5 ) * vh );
	r.y1 = int( ( n.y1 * 0.5 + 0.5 ) * vh );
	(void)margin;
#else
	const double fx0 = std::floor( ( double( n.x0 ) * 0.5 + 0.5 ) * vw ), fx1 = std::ceil( ( double( n.x1 ) * 0.5 + 0.5 ) * vw );
	const double fy0 = std::floor( ( double( n.y0 ) * 0.5 + 0.5 ) * vh ), fy1 = std::ceil( ( double( n.y1 ) * 0.5 + 0.5 ) * vh );
	// far outside the viewport (a huge box): clamp in double before the int
	r.x0 = int( std::clamp( fx0 - margin, -1.0, double( vw ) + 1.0 ) );
	r.x1 = int( std::clamp( fx1 + margin, -1.0, double( vw ) + 1.0 ) );
	r.y0 = int( std::clamp( fy0 - margin, -1.0, double( vh ) + 1.0 ) );
	r.y1 = int( std::clamp( fy1 + margin, -1.0, double( vh ) + 1.0 ) );
#endif
	r.x0 = std::clamp( r.x0, 0, vw );
	r.x1 = std::clamp( r.x1, 0, vw );
	r.y0 = std::clamp( r.y0, 0, vh );
	r.y1 = std::clamp( r.y1, 0, vh );
	if ( r.empty() )
		r = Rect();
	return r;
}

//! what the live layer holds over its static layer, kept per cascade between paints
struct Live
{
	bool known = false;		// the live layer = static layer `gen` except inside `dirty`
	unsigned long long gen = 0;	// the static layer's write it was copied from
	unsigned long long key = 0;	// the cascade key of that paint (the texel lattice of `dirty`)
	Rect dirty;
};

//! a Hit paint may copy back only `dirty` (true) or must copy the whole layer (false)
inline bool partialOk( const Live & l, unsigned long long gen, unsigned long long key, bool red )
{
	return !red && l.known && l.gen == gen && l.key == key;
}

}	// namespace WwSunDyn

#endif
