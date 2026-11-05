/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSSR_H
#define CELLSSR_H

class Scene;

#include "features/ao/ao.h"	// lane MODULES1: the AO pass's targets (WwCellAoTargets) live in the AO module

/*! The game's screen-space reflections: re-draws the opaque cell-lit fragments into a float target, marches
 *  res/shaders/cell_ssr.frag over the AO pass's depth pyramid, and cellSsrMix (cell_ssr.glsl) is the one place the
 *  result enters the picture. Needs the obscurance's pass. Debug: WW_CELL_SSR_RED=off|nogap|nofade,
 *  WW_CELL_SSR_DUMP=<file>. */
void wwCellSsrPass( Scene * scene, bool run );
/*! Rows "SSR" (WW_CELL_SSR, default on) and "SSR outdoors" (WW_CELL_SSR_EXT, ships OFF). */
bool wwCellSsrOn();
void wwCellSsrSetOn( bool on );
bool wwCellSsrExteriorOn();
void wwCellSsrExteriorSetOn( bool on );
//! the draw setupProgram is about to bind: does its material carry the reflection flag
void wwCellSsrNote( bool flagged );
//! called with every program setupProgram binds: the scene pass's masks, and the reflection a flagged draw reads
void wwCellSsrDraw( Scene * scene, bool cellProgram );

/*! On by default (WW_CELL_SSR_PREV=0, WW_CELL_SSR_PREV_RED=1 or WW_PERF3_RED=1: the second scene pass as before).
 *  The reflections draw no second scene pass: the obscurance pass writes the flag into
 *  stencil bit 0x01 of its depth buffer, and a hit samples the previous frame's linear color (kept at the end of
 *  the HDR frame) reprojected into its camera. Falls back to the scene pass when there is no previous frame of
 *  this size and document, when the water's rays or the bounce need the pass. */
bool wwCellSsrPrevStencil();
//! the flag of the draw being set up (wwCellSsrNote)
bool wwCellSsrFlagged();
//! the end of the HDR frame: keep its resolved color and camera for the next frame's reflections
void wwCellSsrKeepFrame( Scene * scene, unsigned int hdrFbo, int w, int h );

/*! Water reflections: when a cell water with the WATR SSR flag draws, wwCellSsrPass also draws the water into a
 *  ray target and the march uses that ray where the water is in front (outdoors the pass runs for water alone).
 *  The water's part has its own row (src/gl/cellssrwater.h), WW_CELL_SSR_WATER / WW/CellSsrWater, ON by default
 *  indoors and outdoors whatever the SSR row; with the SSR row off the pass runs for the water alone and reuses the
 *  previous frame. Red WW_CELL_SSR_WATER_RED=1: the water follows the SSR row as before. */
//! lane FIX1101A: the water ray pass reflects off the flat surface and the colour pass bends the read (water_refl.glsl
//! wwWaterSsrBend); false under WW_CELL_SSR_WATER_RED=1 (the ray from the full normal)
bool wwCellSsrWaterFlat();
//! lane FIX1101A: the water's own SSR part reflects EVERY drawn body (the WATR flag and the Water reflections row
//! aside) and marches with the water twin, at every water preset; false with its row off or under its red
bool wwCellSsrWaterAll();
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

/*! Will this frame's reflections read those targets? The AO opaque pass then runs even with the AO row off. */
bool wwCellSsrWantsTargets( Scene * scene );

#endif
