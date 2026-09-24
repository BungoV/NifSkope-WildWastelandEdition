/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTOROCT_H
#define IMPOSTOROCT_H

/* ---------------------------------------------------------------------------
 * The octahedral impostor's GEOMETRY, and nothing else.
 *
 * Every function here is the drawing half of `docs/LODGEN_IMPOSTOR_SPEC.md`:
 * the direction a frame was photographed from, the frame a direction picks,
 * the three frames a direction falls between, where a frame sits on the sheet,
 * and what the height channel's 0..1 means in world units. The clause each one
 * implements is named in its comment, so a reader can put this file beside the
 * spec line by line.
 *
 * DELIBERATELY FREE OF Qt AND OF OpenGL. Not for tidiness: this file is the
 * one the gate can compile ON ITS OWN (`tests/spells/impostor_draw.sh` builds
 * it into a tiny oracle with g++ and nothing else) and compare, number by
 * number, against the Python reference `tests/spells/impostor_oct_ref.py`. A
 * mapping that only exists inside a shader is a mapping nobody can check.
 *
 * WHERE THE SPEC IS SILENT this file says so IN THE COMMENT, with the word
 * SPEC GAP and the rule that was chosen instead. Three of them are answered
 * not by a proposal but by the BAKE'S OWN CODE (`src/nifskope_ui.cpp:22744`,
 * the `viewDir` lambda): the frame camera's elevation, its azimuth and its
 * roll. Reproducing what the bake did is not the same as the spec saying it,
 * so they are still gaps -- they are just gaps with a determined answer.
 * ------------------------------------------------------------------------- */

namespace ImpostorOct
{

//! The largest grid this reader accepts, matching the bake's own
//! `WW_IMPOSTOR_OCT` bound (`src/nifskope_ui.cpp:22706`: 2..16).
constexpr int kMinGrid = 2;
constexpr int kMaxGrid = 16;

/*! WHICH BAKE A SET CAME FROM.
 *
 *  The spec (226..233) builds the grid on "frame (i, j) is the view from
 *  direction (i, j)". The bake used to photograph frame (i, j) from
 *  ( -d.x, -d.y, +d.z ) instead -- right elevation, azimuth turned by 180
 *  degrees -- because `rz = 90 - azim` where the derivation gives
 *  `rz = 270 - azim`. bungo ruled on 2026-09-19: "Okay, fix the 180 issue".
 *  The BAKE was repaired (src/nifskope_ui.cpp:22755..22757). There is no
 *  toggle and no way back for the bake.
 *
 *  EVERY IMPOSTOR SET BAKED BEFORE THAT EXE MUST BE RE-BAKED.
 *
 *  `SpecLiteral` (the default) is a set from the repaired bake: the grid means
 *  what the spec says. `AsBaked` is a DIAGNOSTIC value that opens a legacy
 *  pre-repair set, and the gate's red control uses it on purpose. A set
 *  declares which it is with the convention token (`card.conv` in the `.lodm`,
 *  `conv <token>` on the manifest `oct` line); a set with no token at all is
 *  legacy, because the token did not exist before the repair.
 */
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

/*! CLAUSE C15 (spec 226..231), verbatim and forward. The direction frame
 *  (i, j) of an N x N grid was photographed from, as a unit vector in the
 *  base's own model space, z up.
 *
 *      u = i/(N-1) * 2 - 1,   v = j/(N-1) * 2 - 1
 *      x = (u+v)/2,  y = (u-v)/2,  z = 1 - |x| - |y|,  normalised
 *
 *  `out` receives x, y, z. False (and `out` untouched) when N is outside
 *  2..16 or (i, j) is outside the grid -- N = 1 would divide by zero and the
 *  spec's own bake refuses it.
 */
bool frameDir( int i, int j, int N, float out[3] );

/*! The INVERSE of C15: the continuous grid coordinate a direction lands on.
 *
 *  SPEC GAP #1 -- the spec gives the forward mapping only. Nothing in it says
 *  how a consumer goes from a camera direction back to (i, j), which is the
 *  first thing a drawer has to do. Proposed wording for the director:
 *
 *    "A consumer maps a direction d (unit, model space, z up, clamped to the
 *     upper hemisphere) to the grid by the inverse of the frame mapping: with
 *     L = |d.x| + |d.y| + d.z, x = d.x/L and y = d.y/L, then u = x + y and
 *     v = x - y, and the continuous grid coordinate is
 *     ((u+1)/2 * (N-1), (v+1)/2 * (N-1))."
 *
 *  It is the algebraic inverse, not a choice: normalising is undone by the L1
 *  projection (|x| + |y| + z = 1 is exactly the octahedron the mapping was
 *  built on), and u = x + y, v = x - y invert x = (u+v)/2, y = (u-v)/2
 *  identically. The only thing it adds is the hemisphere clamp, which the
 *  spec does imply ("the four corners are exact horizon directions"): a
 *  hemi-octahedral grid holds no view from below, so d.z < 0 is clamped to
 *  the horizon rather than wrapped.
 *
 *  `fi`/`fj` come back in [0, N-1]. False when N is out of range or `d` is
 *  degenerate (zero length).
 */
bool dirToGrid( const float d[3], int N, float * fi, float * fj );

/*! The three frames a direction falls between, and their weights.
 *
 *  SPEC GAP #2 -- spec line 231 says "every direction falls inside a triangle
 *  of three frames -- the (N-1)^2 triangle mesh between frame centres is the
 *  blending rule", and then stops. It never says WHICH DIAGONAL splits a grid
 *  cell into its two triangles, and it never says the weights are barycentric
 *  in grid space rather than in direction space. Two consumers can both obey
 *  that sentence and disagree everywhere. Proposed wording:
 *
 *    "Each grid cell is split by the diagonal from (i+1, j) to (i, j+1). With
 *     a and b the fractional position inside the cell, the lower triangle
 *     (a + b <= 1) blends (i,j), (i+1,j), (i,j+1) with weights
 *     (1-a-b, a, b), and the upper triangle blends (i+1,j+1), (i,j+1),
 *     (i+1,j) with weights (a+b-1, 1-a, 1-b). Weights are barycentric in the
 *     GRID, not in direction space, and sum to one."
 *
 *  This is the standard hemi-octahedral method and it is continuous across
 *  the shared diagonal (on a + b = 1 the lower triangle gives (0, a, b) and
 *  the upper gives weight b to (i,j+1) and a to (i+1,j): the same three
 *  numbers on the same two frames). The other diagonal is equally standard
 *  and equally arguable, which is the point -- it has to be WRITTEN DOWN or
 *  the bake and the consumer can disagree with no test able to say who is
 *  wrong.
 *
 *  SPEC GAP #3, found here and not proposed away -- spec line 232 says "the
 *  centre frame the exact top". That is TRUE ONLY FOR ODD N. At N = 4 and
 *  N = 8, the two grids the brief asks for pictures of, (N-1)/2 is not an
 *  integer, there IS no centre frame, and the straight-up direction falls
 *  inside a triangle like any other. The sentence should read "the centre of
 *  the grid is the exact top, and is a frame when N is odd". Nothing in the
 *  code depends on it; a reader does.
 *
 *  `idx[k]` receives the frame index i + j*N (row-major over the sheet, which
 *  is how the bake lays it out, spec 225), `gi`/`gj` the frame's grid
 *  coordinates, `w[k]` the weight. Weights sum to 1 within float error.
 *  False when N is out of range or `d` is degenerate.
 */
bool pickFrames( const float d[3], int N, int idx[3], int gi[3], int gj[3], float w[3] );

/*! The same, for a CAMERA direction rather than a grid direction: applies
 *  `gridLookupDir` first. This is what a drawer calls; `pickFrames` stays
 *  pure so the round-trip property (a frame's own direction picks that frame
 *  with weight 1) can be tested without the convention in the way.
 */
bool pickFramesForCamera( const float camDir[3], int N, int idx[3], int gi[3], int gj[3], float w[3] );

/*! CLAUSE C14 (spec 225): frame (i, j) sits at pixel (i * frameW, j * frameH)
 *  of an N*frameW by N*frameH sheet. In normalised sheet UV that is the rect
 *  (i/N, j/N) .. ((i+1)/N, (j+1)/N).
 *
 *  `u0`, `v0` receive the corner, `du`, `dv` the size (both 1/N). The gutter
 *  is INSIDE this rect and is not subtracted: spec 239 -- "the recorded
 *  halfW/halfH span the full frame, gutter included -- the quad is the
 *  frame". Subtracting it here would shrink the card against its own extents.
 */
bool frameRect( int i, int j, int N, float * u0, float * v0, float * du, float * dv );

/*! CLAUSE C22 (spec 284..286): the `_n` sheet's blue is the window depth of
 *  the orthographic bake, 0.5 is the card plane, and the world offset along
 *  the view direction is (value - 0.5) * depthSpan.
 *
 *  Sign: POSITIVE is AWAY from the camera the frame was photographed from,
 *  because a bake window's depth grows with distance. SPEC GAP #4 -- the spec
 *  gives the magnitude and not the sign, and a consumer that guesses wrong
 *  gets an impostor turned inside out, which is a defect that still looks
 *  like a tree. Proposed wording: "...units = (value - 0.5) x depthspan,
 *  measured ALONG the frame's view direction, away from the camera."
 */
inline float heightToUnits( float h, float depthSpan )
{
	return ( h - 0.5f ) * depthSpan;
}

/*! The frame camera's BASIS in model space: `right`, `up` and `fwd`, where
 *  `fwd` is the direction the camera sits in (the same vector `frameDir`
 *  returns) and `right` / `up` span the frame's picture plane, x to the
 *  right and y up as the sheet stores them.
 *
 *  SPEC GAP #5 -- the spec never says how a frame's picture plane is
 *  oriented, and without that a consumer cannot unproject a texel, cannot
 *  rotate the baked normal out of "the VIEW's space" (spec 282) into the
 *  world, and cannot place the quad. But this one has a DETERMINED answer
 *  rather than a proposed one, because the bake's own code fixes it
 *  (`src/nifskope_ui.cpp:22744`, the `viewDir` lambda): the bake drives the
 *  ordinary viewer camera to the Euler angles
 *
 *      rotX = -90 + elevation,   rotY = 0,   rotZ = 90 - azimuth
 *
 *  with elevation = asin(d.z) and azimuth = atan2(d.y, d.x) in degrees, and
 *  photographs whatever that camera sees. So the frame's basis IS the
 *  viewer's own camera basis at those angles -- including its roll, which is
 *  zero in that convention and therefore NOT the usual "world up projected"
 *  billboard: at the exact top the frame's up SPINS with azimuth, and a
 *  consumer that assumed a projected world up would have every near-vertical
 *  view rotated. Proposed wording for the spec: "A frame's picture plane is
 *  the viewer camera's at rotX = -90 + elevation, rotY = 0, rotZ = 90 -
 *  azimuth, of the frame's own direction; its roll is zero in that
 *  convention, so the frame's up is not the projected world up."
 *
 *  This function reproduces that basis arithmetically. It does NOT call into
 *  the viewer, so the harness has to PIN it against a real camera once
 *  (`WW_IMPOSTOR_PREVIEW` step 2) rather than trust the reproduction -- the
 *  Euler convention of `Matrix::fromEuler` is the kind of thing that is right
 *  until it is not.
 *
 *  False when N/`d` are unusable.
 */
bool frameBasis( const float d[3], float right[3], float up[3], float fwd[3] );

//! The bake's own two angles for a direction, in DEGREES, exactly as
//! `src/nifskope_ui.cpp:22744` computes them: elevation = asin(z),
//! azimuth = atan2(y, x). Exposed so the gate can check the Euler pair the
//! bake would have driven the camera to, and not only the basis derived from
//! it.
bool frameAngles( const float d[3], float * elevDeg, float * azimDeg );

//! Unit length, in place. False (and `v` untouched) when `v` is degenerate.
bool normalise( float v[3] );

/*! CLAUSE C5 (spec 49): normal Z is rebuilt as sqrt(1 - x^2 - y^2), from the
 *  half-packed X and Y the `_n` sheet carries (0.5 = 0). The result is in the
 *  FRAME's view space (spec 282), so a consumer rotates it by that frame's
 *  own basis before blending -- see SPEC GAP #6 in `unpackNormalToModel`.
 */
void unpackNormal( float tx, float ty, float out[3] );

/*! SPEC GAP #6 -- spec 282 says the baked normal is "the geometric normal in
 *  the VIEW's space", which is a statement about the BAKE. It never says what
 *  the consumer does with it, and because the three blended frames have three
 *  DIFFERENT view spaces, the difference is visible: blending the packed X/Y
 *  first and rotating once is wrong wherever the three frames disagree, which
 *  is exactly where a blend is happening. Proposed wording: "A consumer
 *  rotates each frame's normal into model space by THAT frame's basis before
 *  blending, and renormalises the blended result."
 *
 *  This is that rotation for one frame: the unpacked view-space normal turned
 *  by `right`/`up`/`fwd` into the space those vectors are expressed in.
 */
void unpackNormalToModel( float tx, float ty, const float right[3], const float up[3],
	const float fwd[3], float out[3] );

} // namespace ImpostorOct

#endif // IMPOSTOROCT_H
