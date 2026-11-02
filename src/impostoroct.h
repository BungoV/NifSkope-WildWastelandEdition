/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTOROCT_H
#define IMPOSTOROCT_H

// The octahedral impostor's GEOMETRY, and nothing else:
// Every function here is the drawing half of `docs/LODGEN_IMPOSTOR_SPEC.md`: the direction a frame was
// photographed from, the frame a direction picks, the three frames a direction falls between, where a frame
// sits on the sheet, and what the height channel's 0..1 means in world units.

namespace ImpostorOct
{

//! The largest grid this reader accepts, matching the bake's own
//! `WW_IMPOSTOR_OCT` bound (`src/nifskope_ui.cpp:22706`: 2..16).
constexpr int kMinGrid = 2;
constexpr int kMaxGrid = 16;

//! WHICH BAKE A SET CAME FROM:
//! The spec (226..233) builds the grid on "frame (i, j) is the view from direction (i, j)".
enum class Convention { AsBaked = 0, SpecLiteral = 1 };

//! Set from the loaded set's own convention token, or by the preview's
//! `WW_IMPOSTOR_CONVENTION` switch when a person is inspecting one. Process
//! wide, and the drawer sets it per card immediately before a lookup.
extern Convention g_convention;

//! Where the bake's camera stood for the frame whose spec direction is `d`.
//! `SpecLiteral`: `d` itself. `AsBaked` (legacy set): ( -d.x, -d.y, +d.z ).
void bakeCameraDir( const float d[3], float out[3] );

//! The direction to look the grid up with, for a camera sitting at `camDir`.
//! Under `SpecLiteral` it is the identity; under `AsBaked` it undoes the old
//! bake's 180 degrees. Its own inverse either way.
void gridLookupDir( const float camDir[3], float out[3] );

//! CLAUSE C15 (spec 226..231), verbatim and forward. The direction frame (i, j) of an N x N grid was
//! photographed from, as a unit vector in the base's own model space, z up.
bool frameDir( int i, int j, int N, float out[3] );

//! The INVERSE of C15: the continuous grid coordinate a direction lands on.
bool dirToGrid( const float d[3], int N, float * fi, float * fj );

//! The three frames a direction falls between, and their weights.
bool pickFrames( const float d[3], int N, int idx[3], int gi[3], int gj[3], float w[3] );

//! The same, for a CAMERA direction rather than a grid direction: applies `gridLookupDir` first.
bool pickFramesForCamera( const float camDir[3], int N, int idx[3], int gi[3], int gj[3], float w[3] );

//! CLAUSE C14 (spec 225): frame (i, j) sits at pixel (i * frameW, j * frameH) of an N*frameW by N*frameH sheet.
//! In normalised sheet UV that is the rect (i/N, j/N) ..
bool frameRect( int i, int j, int N, float * u0, float * v0, float * du, float * dv );

//! CLAUSE C22 (spec 284..286): the `_n` sheet's blue is the window depth of the orthographic bake, 0.5 is the
//! card plane, and the world offset along the view direction is (value - 0.5) * depthSpan.
inline float heightToUnits( float h, float depthSpan )
{
	return ( h - 0.5f ) * depthSpan;
}

//! The frame camera's BASIS in model space: `right`, `up` and `fwd`, where `fwd` is the direction the camera
//! sits in (the same vector `frameDir` returns) and `right` / `up` span the frame's picture plane, x to the
//! right and y up as the sheet stores them.
bool frameBasis( const float d[3], float right[3], float up[3], float fwd[3] );

//! The bake's own two angles for a direction, in DEGREES, exactly as `src/nifskope_ui.cpp:22744` computes them:
//! elevation = asin(z), azimuth = atan2(y, x).
bool frameAngles( const float d[3], float * elevDeg, float * azimDeg );

//! Unit length, in place. False (and `v` untouched) when `v` is degenerate.
bool normalise( float v[3] );

//! CLAUSE C5 (spec 49): normal Z is rebuilt as sqrt(1 - x^2 - y^2), from the half-packed X and Y the `_n` sheet
//! carries (0.5 = 0).
void unpackNormal( float tx, float ty, float out[3] );

//! SPEC GAP #6 -- spec 282 says the baked normal is "the geometric normal in the VIEW's space", which is a
//! statement about the BAKE.
void unpackNormalToModel( float tx, float ty, const float right[3], const float up[3],
	const float fwd[3], float out[3] );

} // namespace ImpostorOct

#endif // IMPOSTOROCT_H
