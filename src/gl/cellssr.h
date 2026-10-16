/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSSR_H
#define CELLSSR_H

class Scene;

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

/*! Water reflections: when a cell water with the WATR SSR flag draws, wwCellSsrPass also draws the water into a
 *  ray target and the march uses that ray where the water is in front (outdoors the pass runs for water alone). */
//! true while the water ray pass draws; gbufTex = the obscurance pass's target (its .a the opaque view depth)
bool wwCellSsrWaterRayPass( unsigned int * gbufTex );
//! true while either of the reflections' scene passes draws (the water then grabs no frame copy)
bool wwCellSsrInPass();
//! this frame's march and its blurred result for the water (half the view, top row first, bilinear)
bool wwCellSsrWaterTextures( Scene * scene, unsigned int & raw, unsigned int & fin );

//! the AO pass's targets of this frame (src/gl/celllights.cpp); false unless its opaque pass ran for this scene
struct WwCellAoTargets
{
	unsigned int gbuf = 0;		// full size RGBA32F, bottom row first: view normal, linear depth (game units)
	unsigned int depthRb = 0;	// its depth-stencil renderbuffer
	unsigned int mip[5] = {};	// 1..4: min-of-2x2 depth (R32F), top row first
	int w = 0, h = 0, mipW[5] = {}, mipH[5] = {};
};
bool wwCellAoTargets( Scene * scene, WwCellAoTargets & out );
/*! Will this frame's reflections read those targets? The AO opaque pass then runs even with the AO row off. */
bool wwCellSsrWantsTargets( Scene * scene );

#endif
