/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSSR_H
#define CELLSSR_H

class Scene;

/*! THE SCREEN-SPACE REFLECTIONS (lane SSR1, docs/PRTP_PLAN.md "screen-space reflections"): the game's, from its
 *  own shaders. The game draws its opaque frame WITHOUT the reflection term, then, at half the view: a ray per
 *  flagged pixel (the view ray mirrored about the normal, its world z doubled first; kept when it points more
 *  than 0.2 into the view), a march of at most 32 steps over the min-of-2x2 depth mips (4 levels; at the finest
 *  a ray 50 units or more behind the surface is refused), the hit's color with a confidence (screen edge,
 *  travelled distance, 25 x the depth gained / (far - near)), squared; a 5-tap blur across and down where taps
 *  without confidence hand their weight on; and its composite takes lerp(cube, reflection, confidence) in
 *  lane CUBE1's term. A surface carries the flag when its material is environment-mapped and its material file's
 *  reflections switch is on. near = 15; far = the cell's clip distance (XCLL, or its lighting template's).
 *  Here: wwCellSsrPass draws the frame's opaque cell-lit fragments once more (probe 60: the lit color without
 *  reflection, obscurance and fog; alpha = the flag) into a full-size float target, runs
 *  res/shaders/cell_ssr.frag over it and lane AO1's depth pyramid (one pyramid, not two), and the cell programs
 *  mix the bilinear sample into their cube term: cellSsrMix in res/shaders/cell_ssr.glsl is the ONE place the
 *  reflection enters the picture. Interiors only; part of the Cell lights row. The opaque pass and depth pyramid
 *  run whenever SSR is on, whatever the Ambient Occlusion row or method (lane SSAO2: WW_CELL_AO_SSR_RED=old drops them).
 *  The march reads LINEAR light, before the imagespace's tone map (the cell view applies that per fragment,
 *  after the point the scene pass taps). When the frame becomes one linear target with a single tone map, it
 *  should read that same opaque linear frame before the reflection term, and cellSsrMix moves with the cube term.
 *  ASSUMED, not read from the game: (1) the four scales and near / far sit in the constant rows in the order
 *  their uses imply; (2) the depth pyramid the march loads is the obscurance's; (3) the ray pass point-samples
 *  its inputs; (4) the composite reads the blurred result bilinearly. DEVIATIONS: float targets (the game
 *  keeps the march and the blurs in 8 bits); the color marched is the cell view's own opaque frame (its
 *  emissive is added after the square root, lane CUBE1's form); a material set in a NIF without a material
 *  file carries no flag; exteriors are not done.
 *  Pins: WW_CELL_SSR_RED=off (computed, not applied) | nogap (no 50-unit refusal) | nofade (confidence 1 on a
 *  hit), WW_CELL_SSR_DUMP=<file> (the inputs, the ray, the march, the blurred result, the numbers;
 *  tests/spells/cell_ssr.sh). Probe 61: the reflection as a draw read it. */
void wwCellSsrPass( Scene * scene, bool run );
/*! Lane CELLALL1: the rows. "SSR" (WW/CellSsr, env pin WW_CELL_SSR, default on: the interiors' reflections as
 *  they were) and "SSR outdoors" (WW/CellSsrExterior, env pin WW_CELL_SSR_EXT, ships OFF): an exterior marches
 *  the same pass (its far = the cell's clip distance, else the ceiling). Water is lane WATER2's. */
bool wwCellSsrOn();
void wwCellSsrSetOn( bool on );
bool wwCellSsrExteriorOn();
void wwCellSsrExteriorSetOn( bool on );
//! the draw setupProgram is about to bind: does its material carry the reflection flag
void wwCellSsrNote( bool flagged );
//! called with every program setupProgram binds: the scene pass's masks, and the reflection a flagged draw reads
void wwCellSsrDraw( Scene * scene, bool cellProgram );

/*! Lane PERF3, on since PERF5 (a speed change; WW_CELL_SSR_PREV=0, WW_CELL_SSR_PREV_RED=1 or WW_PERF3_RED=1: the
 *  second scene pass as before). The reflections draw no second scene pass: the obscurance pass writes the flag into
 *  stencil bit 0x01 of its depth buffer, and a hit samples the previous frame's linear color (kept at the end of
 *  the HDR frame) reprojected into its camera. Falls back to the scene pass when there is no previous frame of
 *  this size and document, when the water's rays or the bounce need the pass. */
bool wwCellSsrPrevStencil();
//! the flag of the draw being set up (wwCellSsrNote)
bool wwCellSsrFlagged();
//! the end of the HDR frame: keep its resolved color and camera for the next frame's reflections
void wwCellSsrKeepFrame( Scene * scene, unsigned int hdrFbo, int w, int h );

/*! Lane WATER2: THE WATER'S REFLECTIONS (the game's water ray pass, asm 02100; the water reads them in 02102).
 *  When a cell water surface draws (src/gl/cellwater.h) and its WATR's SSR flag is set, wwCellSsrPass also
 *  draws the water once more into a full-size ray target (fo4_water.frag with waterSslr = 1: the ray into the
 *  view and the view depth), behind the opaque depth, and the ray stage takes that ray where the water lies in
 *  front. In an exterior the pass then runs for the water alone: no opaque draw reads the reflections there. */
//! true while the water ray pass draws; gbufTex = the obscurance pass's target (its .a the opaque view depth)
bool wwCellSsrWaterRayPass( unsigned int * gbufTex );
//! true while either of the reflections' scene passes draws (the water then grabs no frame copy)
bool wwCellSsrInPass();
//! lane SSGI1: this frame's scene pass (full size RGBA32F, bottom row first), drawn for the reflections or the bounce
bool wwCellSsrSceneTexture( Scene * scene, unsigned int & tex );
//! lane SSGI2: the same pass's emitted share (full size RGBA16F, a = coverage, mips 0..4); only while the bounce's
//! emissive cast wants it (wwCellSsgiWantsEmission)
bool wwCellSsrEmissionTexture( Scene * scene, unsigned int & tex );
//! this frame's march and its blurred result for the water (half the view, top row first, bilinear)
bool wwCellSsrWaterTextures( Scene * scene, unsigned int & raw, unsigned int & fin );

//! lane AO1's targets of this frame (src/gl/celllights.cpp); false unless its opaque pass ran for this scene
struct WwCellAoTargets
{
	unsigned int gbuf = 0;		// full size RGBA32F, bottom row first: view normal, linear depth (game units)
	unsigned int depthRb = 0;	// its depth-stencil renderbuffer
	unsigned int mip[5] = {};	// 1..4: min-of-2x2 depth (R32F), top row first
	int w = 0, h = 0, mipW[5] = {}, mipH[5] = {};
};
bool wwCellAoTargets( Scene * scene, WwCellAoTargets & out );
/*! lane AOOFF1: will this frame's reflections (the full pass or the water's) read those targets? The obscurance's
 *  opaque pass is the SHARED g-buffer: it runs while the AO row is off whenever this says yes */
bool wwCellSsrWantsTargets( Scene * scene );

#endif
