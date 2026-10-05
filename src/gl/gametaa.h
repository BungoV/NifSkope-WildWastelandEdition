#ifndef GAMETAA_H
#define GAMETAA_H

#include <QString>

class Matrix4;
class Scene;
class Transform;

/*! Lane MOTION1 (2026-10-04): THE GAME'S TEMPORAL AA, rebuilt from Todd's treat and Nomad's shader exports.
 *
 *  Every constant below was read from the game, none chosen (the lane notes hold the trace):
 *   - jitter: an 8-frame cycle, frame = (frame + 1) & 7, n = frame + 1; the projection's clip xy move by
 *     ( (2 Halton(2, n) - 1) / W, (2 Halton(3, n) - 1) / H ) times w, i.e. (Halton - 0.5) pixels, x right and
 *     y up. Halton is the game's float radical inverse (1/base, fmodf, floor) step for step;
 *   - motion vectors from the UNJITTERED matrices: here the camera's own reprojection of the frame's depth
 *     (this viewer draws still scenes; a moving object would need per-object vectors, which nothing here has);
 *   - the resolve: the closest-depth 3x3 dilation of the motion vector, a luma-only history
 *     (Y = .25 R + .5 G + .25 B, the motion length beside it), the history colour rebuilt from the two
 *     neighbours that bracket the history's luma (that IS the neighbourhood clamp), the history weight
 *     min(1 - 20 |dv|, lerp(fTAAHighFreq 0.8, fTAALowFreq 0.5, v)), the 2x2 tent resample (fTAASharpen 1.0),
 *     fTAAPostSharpen 0.21, fTAAPostOverlay 0.21, every output saturated: the pass runs on the display frame.
 *  res/shaders/game_taa.frag is the resolve, instruction for instruction; game_taa_mv.frag the vectors.
 *
 *  ONE row, two places: "Temporal AA" in the Scene popup's Effects (the model preview) and in the Cell
 *  workspace's PRTP band (the cell view). Ships OFF, persisted. OFF touches nothing: no target, no jitter, no
 *  pass, so an OFF frame is the before-lane frame byte for byte. A pick render never takes it.
 *
 *  Pins (a harness without WW_TAA is OFF whatever was saved):
 *   WW_TAA=0|1                 the row
 *   WW_TAA_RED=noclamp         the history luma is not clamped into its bracket (the gate's red)
 *   WW_TAA_RED=wrongjitter     Halton bases swapped (3, 2) (the gate's red)
 *   WW_TAA_DUMP=<dir>          at frame WW_TAA_DUMP_FRAME (the frame index, default 8): every input and
 *                              output of the resolve, raw, plus the numbers (taa.txt) -- the numpy rebuild's food
 *  Divergences, stated: the history target is RGBA16F and the vectors RG32F (the game's formats are owed to a
 *  capture); the vectors are the camera's alone; no dynamic resolution (c0.zw = c5.zw = 1). */

//! The row's state (pinned, saved, or OFF in a harness run).
bool wwGameTaaOn();
//! The row: sets and saves (unless pinned).
void wwGameTaaSetOn( bool on );

/*! The frame index the next frames use: >= 0 pins it (the camera path renders frame k as index k, so a
 *  repeated paint of the same frame is the same picture), -1 (default) counts paints. */
void wwGameTaaSetFrame( int index );

//! paintGL arms the jitter for its own projection only (not a pick, not a tool's).
void wwGameTaaArmJitter( bool armed );
//! glProjection: jitters the matrix when armed and on; keeps the unjittered one for the vectors.
void wwGameTaaJitterProjection( Scene * scene, Matrix4 & proj );
//! The projection without the jitter (the shadow fit reads the camera's frustum, not the jittered one).
bool wwGameTaaUnjittered( Matrix4 & proj );
//! Lane GRASSMB1: this frame's jitter in NDC (the offsets the projection got); false when not jittered.
bool wwGameTaaJitter( float & offX, float & offY );

//! After the scene's draw (and the cell view's one tone map): the vectors, the resolve, the frame back.
void wwGameTaaResolve( Scene * scene );
//! True when the view moved recently: an interactive window paints a few more frames to settle.
bool wwGameTaaWantsSettle();

//! One line for the notes / the path log: the last frame's index, n, offsets, or why it did not run.
QString wwGameTaaEcho();

#endif
