/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "impostoroct.h"

#include <cmath>

/* No Qt, no OpenGL, no project headers beyond its own -- see impostoroct.h for
 * why. `tests/spells/impostor_draw.sh` compiles this file and nothing else
 * into an oracle and diffs it against `tests/spells/impostor_oct_ref.py`. */

namespace ImpostorOct
{

static const float kPi = 3.14159265358979323846f;

/* ---------------------------------------------------------------------------
 * THE BAKE CAMERA, AND THE 180 DEGREES NOBODY HAD SEEN (repaired 2026-09-19).
 *
 * `Matrix::fromEuler` (src/data/niftypes.cpp:215) with rotY = 0 has rows
 *
 *     row0 = ( cosZ, -sinZ, 0 )
 *     row1 = ( sinZ cosX, cosX cosZ, -sinX )
 *     row2 = ( sinX sinZ, sinX cosZ, cosX )
 *
 * and the rotation is the view transform's (GLView::viewTransform,
 * src/glview.cpp:7029), so its ROWS are the camera's axes in world space.
 * Putting the six axis views of `GLView::viewRotations` (src/glview.cpp:187)
 * through it settles the convention with no guessing left in it:
 *
 *     Top ( 0, 0, 0 )        row2 = +Z        Bottom ( 180, 0, 0 )  row2 = -Z
 *     Left ( -90, 0, -90 )   row2 = +X        Right ( -90, 0, 90 )  row2 = -X
 *     Front ( -90, 0, 180 )  row2 = +Y        Back ( -90, 0, 0 )    row2 = -Y
 *
 * Six for six: the camera SITS at +row2, `up` is row1 -- which comes out as
 * the world +Z projected perpendicular to row2, i.e. roll zero -- and
 * row0 = up x fwd exactly. There is no sign left to guess, in either vector.
 *
 * Now put the BAKE's own angles through the same rows. Until 2026-09-19 the
 * bake drove the camera to rotX = -90 + elevation, rotY = 0,
 * rotZ = 90 - azimuth, with elevation = asin(d.z) and
 * azimuth = atan2(d.y, d.x) of the frame direction d. For every direction,
 * every elevation and every azimuth, that yields
 *
 *     row2 = ( -d.x, -d.y, +d.z )
 *
 * -- the elevation is right and THE AZIMUTH IS TURNED BY 180 DEGREES. So
 * frame (i, j) did not hold the view from `frameDir(i, j)`. It held the
 * view from the opposite side of the object.
 *
 * That was a defect in the bake against its own spec (spec 226..233 builds the
 * whole grid on frame (i, j) BEING direction (i, j)), and the reason it
 * survived is that nothing but trees had ever been baked: a maple is near
 * enough symmetric about its axis that the wrong side looks like the right
 * one. It was invisible until you put the card beside the mesh -- which is
 * exactly what this lane's gate does -- or until somebody baked a car.
 *
 * RULED AND REPAIRED, bungo 2026-09-19: "Okay, fix the 180 issue". The bake
 * now drives `rz = 270 - azim` (src/nifskope_ui.cpp), and the DERIVATION,
 * because a one-liner is not a proof:
 *
 *     the camera sits at row2 = ( sinX sinZ, sinX cosZ, cosX )
 *     want row2 = d = ( cos(elev) cos(azim), cos(elev) sin(azim), sin(elev) )
 *
 *     rx = -90 + elev  =>  cosX = sin(elev)          the elevation was already right
 *                          sinX = -cos(elev)
 *     so we need        sinZ = -cos(azim),  cosZ = -sin(azim)
 *     and               Z = 270 - azim  gives  sin = -cos(azim), cos = -sin(azim)   OK
 *     while the old     Z =  90 - azim  gives  sin = +cos(azim), cos = +sin(azim)
 *                       i.e. row2 = ( -d.x, -d.y, +d.z ): the azimuth turned by 180.
 *
 * `Convention` now names WHICH BAKE A SET CAME FROM, not a preference:
 *   SpecLiteral (the default) -- a set baked by this exe or later. The
 *       `.lodm` says so with `card.conv` and the manifest `oct` line with a
 *       trailing `conv <token>`.
 *   AsBaked -- a LEGACY set, baked before the repair, with no token. It is a
 *       diagnostic value so such a set can still be opened and looked at; it
 *       is not a way back for the bake and nothing writes it.
 *
 * EVERY IMPOSTOR SET BAKED BEFORE THIS EXE MUST BE RE-BAKED.
 * ------------------------------------------------------------------------- */

Convention g_convention = Convention::SpecLiteral;

void bakeCameraDir( const float d[3], float out[3] )
{
	/* Where the bake's camera stood for the frame whose spec direction is `d`.
	 * After the repair that IS `d`; for a legacy set it is the azimuth turned
	 * by 180. Kept as a function rather than folded away because the gate
	 * measures the legacy case on purpose -- a red control that cannot be
	 * expressed cannot be run. */
	if ( g_convention == Convention::AsBaked ) {
		out[0] = -d[0];
		out[1] = -d[1];
		out[2] =  d[2];
	} else {
		out[0] = d[0];
		out[1] = d[1];
		out[2] = d[2];
	}
}

void gridLookupDir( const float camDir[3], float out[3] )
{
	if ( g_convention == Convention::AsBaked ) {
		/* Undo the bake's 180 degrees so the frame we pick is the frame that
		 * actually holds this view. Its own inverse, which is why one
		 * function serves both directions. */
		out[0] = -camDir[0];
		out[1] = -camDir[1];
		out[2] =  camDir[2];
	} else {
		out[0] = camDir[0];
		out[1] = camDir[1];
		out[2] = camDir[2];
	}
}

bool normalise( float v[3] )
{
	const float n = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( !( n > 1e-20f ) )
		return false;
	v[0] /= n; v[1] /= n; v[2] /= n;
	return true;
}

static bool gridOk( int N )
{
	return N >= kMinGrid && N <= kMaxGrid;
}

bool frameDir( int i, int j, int N, float out[3] )
{
	if ( !gridOk( N ) || i < 0 || j < 0 || i >= N || j >= N )
		return false;
	/* CLAUSE C15, spec 226..231, transcribed and not paraphrased. */
	const float u = float( i ) / float( N - 1 ) * 2.0f - 1.0f;
	const float v = float( j ) / float( N - 1 ) * 2.0f - 1.0f;
	float d[3];
	d[0] = ( u + v ) * 0.5f;
	d[1] = ( u - v ) * 0.5f;
	d[2] = 1.0f - std::fabs( d[0] ) - std::fabs( d[1] );
	if ( !normalise( d ) )
		return false;
	out[0] = d[0]; out[1] = d[1]; out[2] = d[2];
	return true;
}

bool dirToGrid( const float d[3], int N, float * fi, float * fj )
{
	if ( !gridOk( N ) || !fi || !fj )
		return false;
	float e[3] = { d[0], d[1], d[2] };
	if ( !normalise( e ) )
		return false;
	/* The hemisphere clamp. A hemi-octahedral grid holds no view from below
	 * the horizon, so a direction from below is answered with the horizon
	 * direction under it rather than with a wrapped frame that would show the
	 * tree's top from underneath. A camera exactly level with the card lands
	 * on the grid's outer edge, which is where the spec puts the horizon. */
	if ( e[2] < 0.0f ) {
		e[2] = 0.0f;
		if ( !normalise( e ) )
			return false;
	}
	const float L = std::fabs( e[0] ) + std::fabs( e[1] ) + e[2];
	if ( !( L > 1e-20f ) )
		return false;
	const float x = e[0] / L;
	const float y = e[1] / L;
	/* SPEC GAP #1: the algebraic inverse of x = (u+v)/2, y = (u-v)/2. */
	const float u = x + y;
	const float v = x - y;
	float a = ( u + 1.0f ) * 0.5f * float( N - 1 );
	float b = ( v + 1.0f ) * 0.5f * float( N - 1 );
	/* Rounding at the rim can put a legal horizon direction a hair outside the
	 * grid; clamping is not a fudge of the mapping, it is the mapping's own
	 * closed interval. */
	const float hi = float( N - 1 );
	if ( a < 0.0f ) a = 0.0f; else if ( a > hi ) a = hi;
	if ( b < 0.0f ) b = 0.0f; else if ( b > hi ) b = hi;
	*fi = a;
	*fj = b;
	return true;
}

bool pickFrames( const float d[3], int N, int idx[3], int gi[3], int gj[3], float w[3] )
{
	float fi = 0.0f, fj = 0.0f;
	if ( !dirToGrid( d, N, &fi, &fj ) )
		return false;

	int i0 = int( std::floor( fi ) );
	int j0 = int( std::floor( fj ) );
	if ( i0 > N - 2 ) i0 = N - 2;
	if ( j0 > N - 2 ) j0 = N - 2;
	if ( i0 < 0 ) i0 = 0;
	if ( j0 < 0 ) j0 = 0;

	const float a = fi - float( i0 );
	const float b = fj - float( j0 );

	/* SPEC GAP #2: the cell's diagonal runs (i+1, j) -- (i, j+1), and the
	 * weights are barycentric in the GRID. Continuous across the diagonal:
	 * on a + b == 1 the lower branch gives ( 0, a, b ) and the upper gives
	 * weight b to (i, j+1) and a to (i+1, j), which is the same picture. */
	if ( a + b <= 1.0f ) {
		gi[0] = i0;     gj[0] = j0;     w[0] = 1.0f - a - b;
		gi[1] = i0 + 1; gj[1] = j0;     w[1] = a;
		gi[2] = i0;     gj[2] = j0 + 1; w[2] = b;
	} else {
		gi[0] = i0 + 1; gj[0] = j0 + 1; w[0] = a + b - 1.0f;
		gi[1] = i0;     gj[1] = j0 + 1; w[1] = 1.0f - a;
		gi[2] = i0 + 1; gj[2] = j0;     w[2] = 1.0f - b;
	}
	for ( int k = 0; k < 3; k++ ) {
		if ( w[k] < 0.0f )
			w[k] = 0.0f;
		/* Row-major over the sheet: frame (i, j) is at pixel
		 * (i * frameW, j * frameH), so i runs along x and j along y. */
		idx[k] = gi[k] + gj[k] * N;
	}
	return true;
}

bool pickFramesForCamera( const float camDir[3], int N, int idx[3], int gi[3], int gj[3], float w[3] )
{
	float g[3];
	gridLookupDir( camDir, g );
	return pickFrames( g, N, idx, gi, gj, w );
}

bool frameRect( int i, int j, int N, float * u0, float * v0, float * du, float * dv )
{
	if ( !gridOk( N ) || i < 0 || j < 0 || i >= N || j >= N || !u0 || !v0 || !du || !dv )
		return false;
	const float s = 1.0f / float( N );
	*u0 = float( i ) * s;
	*v0 = float( j ) * s;
	*du = s;
	*dv = s;
	return true;
}

bool frameAngles( const float d[3], float * elevDeg, float * azimDeg )
{
	if ( !elevDeg || !azimDeg )
		return false;
	float e[3] = { d[0], d[1], d[2] };
	if ( !normalise( e ) )
		return false;
	float z = e[2];
	if ( z < -1.0f ) z = -1.0f; else if ( z > 1.0f ) z = 1.0f;
	/* The bake's own two lines, src/nifskope_ui.cpp:22748..22750. */
	*elevDeg = std::asin( z ) * 180.0f / kPi;
	*azimDeg = std::atan2( e[1], e[0] ) * 180.0f / kPi;
	return true;
}

bool frameBasis( const float d[3], float right[3], float up[3], float fwd[3] )
{
	if ( !right || !up || !fwd )
		return false;
	float elev = 0.0f, azim = 0.0f;
	if ( !frameAngles( d, &elev, &azim ) )
		return false;

	/* The camera the bake drove for THIS frame: rotX = -90 + elevation,
	 * rotY = 0, rotZ = 270 - azimuth (src/nifskope_ui.cpp:22755..22757, the
	 * repaired form -- the derivation is in the block at the top of this file).
	 * The basis is that camera's, whatever it points at: a drawer has to sample
	 * the SHEET, so for a legacy set baked before the repair the old 90 - azim
	 * is still the honest answer and `AsBaked` selects it. */
	const float rx = ( -90.0f + elev ) * kPi / 180.0f;
	const float rz = ( ( g_convention == Convention::AsBaked ? 90.0f : 270.0f )
					   - azim ) * kPi / 180.0f;

	const float sinX = std::sin( rx ), cosX = std::cos( rx );
	const float sinZ = std::sin( rz ), cosZ = std::cos( rz );

	/* Matrix::fromEuler with rotY = 0 (src/data/niftypes.cpp:224..232);
	 * its rows are the camera axes in world space, measured against all six
	 * of GLView::viewRotations -- see the block at the top of this file. */
	right[0] = cosZ;             right[1] = -sinZ;           right[2] = 0.0f;
	up[0]    = sinZ * cosX;      up[1]    = cosX * cosZ;     up[2]    = -sinX;
	fwd[0]   = sinX * sinZ;      fwd[1]   = sinX * cosZ;     fwd[2]   = cosX;

	normalise( right );
	normalise( up );
	normalise( fwd );
	return true;
}

void unpackNormal( float tx, float ty, float out[3] )
{
	/* CLAUSE C5, spec 49: half-packed X and Y, Z rebuilt as the positive root
	 * of 1 - x^2 - y^2. A sheet texel can exceed the unit disc after BC3
	 * quantisation, so the radicand is floored rather than allowed to produce
	 * a NaN that would poison the blend. */
	const float nx = tx * 2.0f - 1.0f;
	const float ny = ty * 2.0f - 1.0f;
	float r = 1.0f - nx * nx - ny * ny;
	if ( r < 0.0f )
		r = 0.0f;
	out[0] = nx;
	out[1] = ny;
	out[2] = std::sqrt( r );
}

void unpackNormalToModel( float tx, float ty, const float right[3], const float up[3],
	const float fwd[3], float out[3] )
{
	float n[3];
	unpackNormal( tx, ty, n );
	/* SPEC GAP #6: out of THIS frame's view space, before any blending. */
	for ( int k = 0; k < 3; k++ )
		out[k] = right[k] * n[0] + up[k] * n[1] + fwd[k] * n[2];
	normalise( out );
}

} // namespace ImpostorOct
