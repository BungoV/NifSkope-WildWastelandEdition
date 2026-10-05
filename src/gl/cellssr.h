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
 *  reflection enters the picture. Interiors only; part of the Cell lights row; needs the obscurance's pass
 *  (WW_CELL_AO=0 leaves none).
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
//! the draw setupProgram is about to bind: does its material carry the reflection flag
void wwCellSsrNote( bool flagged );
//! called with every program setupProgram binds: the scene pass's masks, and the reflection a flagged draw reads
void wwCellSsrDraw( Scene * scene, bool cellProgram );

/*! Lane WATER2: THE WATER'S REFLECTIONS (the game's water ray pass, asm 02100; the water reads them in 02102).
 *  When a cell water surface draws (src/gl/cellwater.h) and its WATR's SSR flag is set, wwCellSsrPass also
 *  draws the water once more into a full-size ray target (fo4_water.frag with waterSslr = 1: the ray into the
 *  view and the view depth), behind the opaque depth, and the ray stage takes that ray where the water lies in
 *  front. In an exterior the pass then runs for the water alone: no opaque draw reads the reflections there. */
//! true while the water ray pass draws; gbufTex = the obscurance pass's target (its .a the opaque view depth)
bool wwCellSsrWaterRayPass( unsigned int * gbufTex );
//! true while either of the reflections' scene passes draws (the water then grabs no frame copy)
bool wwCellSsrInPass();
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

#endif
