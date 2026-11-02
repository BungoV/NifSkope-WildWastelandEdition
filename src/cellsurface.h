#ifndef CELLSURFACE_H
#define CELLSURFACE_H

/*
 * Lane ESPWRITE2 (2026-11-02): the cell view's SURFACE PICK -- the point and normal under a pixel, for placing a
 * reference (src/celledit.h). Two stages:
 *   1. a one-pixel depth readback of the frame just drawn (the overlay point of GLView::paintGL), unprojected with
 *      the frame's own near / far planes along the pixel's world ray: what is DRAWN there, at any scene size;
 *   2. a triangle refine: the ray against the shapes whose bound holds the depth point only, for the exact point and
 *      the triangle's normal. The refine is accepted when it lands within max( 2 u, 1 % ) of the depth point.
 * Depth unreadable (a multisampled target) or the sky: the whole-scene ray (GLView::raycastScene, the 3D cursor's
 * path) answers instead; the counters say how often each path answered (cellSurfaceStatsLine).
 *
 * Red WW_CELLSURF_RED (read on every call): `ray` = the old path only (the whole-scene ray, no depth);
 * `lin` = a linear depth unprojection (wrong for a perspective frustum; tests/cellsurface must fail on it).
 *
 * The maths here is pure (no GL, no Qt GUI) so tests/cellsurface links it alone.
 */

#include <QString>

struct CellSurfaceHit
{
	bool ok = false;
	float pos[3] = { 0, 0, 0 };
	float normal[3] = { 0, 0, 1 };
	int via = 0;            //!< 1 depth + triangle refine, 2 depth only, 3 whole-scene ray (fallback or red)
	float tDepth = 0.0f;    //!< ray distance from the depth readback (0 when none)
	float tRefine = 0.0f;   //!< ray distance of the refined triangle (0 when none)
	float depth = 1.0f;     //!< the raw depth value read
};

struct CellSurfaceStats
{
	int probes = 0, refined = 0, depthOnly = 0, rayFallback = 0, sky = 0, unreadable = 0, missed = 0;
};

//! WW_CELLSURF_RED, read on every call
QString cellSurfaceRed();

/*! The eye-space distance (along the view axis) of a [0,1] depth value under a standard frustum (persp) or an
 *  orthographic box. False for the far plane (sky) and for values outside (0, 1). */
bool cellSurfaceEyeDistance( float depth, double nearT, double farT, bool persp, double & zEye );

/*! The ray parameter (normalised dir) at eye distance zEye: zEye / dot( dir, forward ). False when the ray runs
 *  parallel to the view plane. */
bool cellSurfaceRayParam( const float dir[3], const float forward[3], double zEye, double & t );

//! Moller-Trumbore, both faces; t > 0 along dir.
bool cellSurfaceRayTri( const float o[3], const float d[3], const float a[3], const float b[3], const float c[3],
	float & t );

//! The unit normal of (a, b, c), flipped to face against the ray dir. False for a degenerate triangle.
bool cellSurfaceTriNormal( const float a[3], const float b[3], const float c[3], const float rayDir[3], float n[3] );

//! The refine tolerance at ray distance t: max( 2 u, 1 % of t ).
float cellSurfaceTolerance( float t );

//! The frame's planes, noted by GLView's projection set-up every frame.
void cellSurfaceNotePlanes( double nearT, double farT, bool persp );
void cellSurfacePlanes( double & nearT, double & farT, bool & persp );

CellSurfaceStats & cellSurfaceStats();
QString cellSurfaceStatsLine();

#endif // CELLSURFACE_H
