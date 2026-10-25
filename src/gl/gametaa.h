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
 *  Divergences, stated: the history target is RGBA16F and the vectors RG32F (lane TAA2 MOTION1-FMT: the game's
 *  vectors are R16G16_FLOAT and its frame R11G11B10_FLOAT before the pass, RGBA8 after it, per FO4CS's RenderDoc
 *  notes; its history's format is still owed to a capture); the vectors are the camera's alone; no dynamic
 *  resolution (c0.zw = c5.zw = 1).
 *
 *  Lane TAA2 (2026-10-23): "Modern temporal AA", Off | Low | Medium | High | Ultra, beside the row above. A tier
 *  above Off jitters the frame (the same Halton) and replaces the game's resolve with res/shaders/taa2.frag
 *  (full-colour history, jitter-aware current filter, Catmull-Rom history, variance clip, anti-flicker) and, at
 *  High and Ultra, AMD's RCAS (taa2_rcas.frag, MIT). Off = the game's path above, untouched. Ships Off, saved.
 *   WW_TAA2=0..4|off|low|medium|high|ultra  the tier (a harness without it: Off)
 *   WW_TAA2_RED=noclip|nobicubic|wrongjitter  the gates' reds (no clip; bilinear history; the filter's jitter turned)
 *   WW_TAA2_DUMP=<dir>         the frame's 256 px centre crop, out and in, for 32 frames from WW_TAA_DUMP_FRAME
 *  The resolve and its final pass are GPU-timed: a "modern temporal aa: tier N gpu X ms" line every 60 frames.
 *
 *  Lane TAA3 (2026-10-25): every tier keeps a thin-feature lock (taa2.frag o2; AMD FSR 2's idea) so a sub-pixel
 *  wire standing still keeps its history; Ultra reads TAAINPUTS1's velocity and mask (gl/taainputs.h); the row
 *  "Modern temporal AA before tone map" (ships Off, saved) resolves the cell's linear frame before its tone map.
 *   WW_TAA2_RED=...|nolock|noinputs  more reds (no lock; Ultra without the inputs)
 *   WW_TAA2_HDR=0|1            the before-tone-map row (a harness without it: Off) */

//! True when either resolve runs (the game's row or a Modern tier): the frame is jittered.
bool wwGameTaaOn();
//! The game's "Temporal AA" row alone (pinned, saved, or OFF in a harness run).
bool wwGameTaaRowOn();
//! The row: sets and saves (unless pinned).
void wwGameTaaSetOn( bool on );

//! lane TAA2: the "Modern temporal AA" tier, 0 Off .. 4 Ultra; the setter saves it unless WW_TAA2 pins it.
int wwTaa2Tier();
void wwTaa2SetTier( int tier );
bool wwTaa2Pinned();
//! lane TAA3: the "Modern temporal AA before tone map" row; the setter saves it unless WW_TAA2_HDR pins it.
bool wwTaa2HdrOn();
void wwTaa2SetHdrOn( bool on );

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
/*! lane TAA3: the cell's linear frame (fbo, its colour format) before the tone map, when the row is on and a Modern
 *  tier runs; expose scales it to about display level. The later wwGameTaaResolve then only ends the frame. */
void wwGameTaaResolveLinear( Scene * scene, unsigned fbo, unsigned format, float expose );
//! lane TAA3: true (once) when the linear resolve already ran this frame's TAAINPUTS1 pass.
bool wwGameTaaInputsRan();
//! True when the view moved recently: an interactive window paints a few more frames to settle.
bool wwGameTaaWantsSettle();

//! lane VRS1: the last resolve's vectors (RG32F, previous minus current, texture units) and their size; 0 when none
unsigned wwGameTaaMotionTex( Scene * scene, int & w, int & h );

//! One line for the notes / the path log: the last frame's index, n, offsets, or why it did not run.
QString wwGameTaaEcho();

#endif
