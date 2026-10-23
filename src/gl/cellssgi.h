/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSSGI_H
#define CELLSSGI_H

class Scene;

/*! Lane SSGI1: SCREEN-SPACE GI for the cell view, the method of our FO4CS's SSGI (notes/ssgi1/FO4CS_METHOD.md) in our
 *  own GLSL (res/shaders/cell_ssgi.frag): a visibility-bitmask march over lane AO1's normal + depth, lit by lane
 *  SSR1's scene pass (the opaque lit linear colour), then temporal accumulation and a depth/normal-aware denoise.
 *  The surfaces read it in cell_lights.glsl (cellSsgiE, cellSsgiAo): the probe GI keeps only the share of the
 *  hemisphere the march left open (FO4CS's probe-fill law), the screen bounce is added, and the visibility joins the
 *  obscurance by min (FO4CS's composite). Row "SSGI" (WW/CellSsgi, pin WW_RENDER_SSGI) ships OFF; "SSGI quality"
 *  (WW/CellSsgiQuality, pin WW_CELL_SSGI_QUALITY=low|medium|high|ultra) picks FO4CS's tiers.
 *  Reds: WW_CELL_SSGI_RED=off (computed, not applied) | stack (added on top of the probes: the double count) |
 *  notemporal | noblur. */
bool wwCellSsgiOn();
void wwCellSsgiSetOn( bool on );
int wwCellSsgiQuality();			// 0 Low, 1 Medium, 2 High, 3 Ultra
void wwCellSsgiSetQuality( int q );
bool wwCellSsgiPinned();
//! does this frame want lane SSR1's scene pass and lane AO1's g-buffer for the bounce
bool wwCellSsgiWantsScene( Scene * scene );
//! lane SSGI2: the scene pass also writes the emitted share (the emissive cast; red `noemissive` off)
bool wwCellSsgiWantsEmission( Scene * scene );
//! after wwCellSsrPass: the cast, the history, the denoise
void wwCellSsgiPass( Scene * scene, bool run );
//! called with every program setupProgram binds: the bounce a cell-lit opaque draw reads
void wwCellSsgiDraw( Scene * scene, bool cellProgram );
//! one line for the notes
const char * wwCellSsgiEcho();

#endif
