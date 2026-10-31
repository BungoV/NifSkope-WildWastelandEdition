#ifndef CELLHDR_H
#define CELLHDR_H

#include <QString>

class Scene;

/*! The cell view's linear frame: cell programs write linear light into a float target (stencil 1, others 2) and
 *  one pass (cell_hdr.prog) runs the imagespace once on the sum, as the game does, so stacked additive effects do
 *  not each get tone-mapped. WW_CELL_HDR_RED=perfrag keeps a per-fragment tone map (debug control). */

//! Before the main scene draw: binds the linear frame when this frame takes it; true when it did.
bool wwCellHdrBegin( Scene * scene );
//! After the main scene draw: the one tone map, back into the frame bound before wwCellHdrBegin.
void wwCellHdrEnd( Scene * scene );
//! True between wwCellHdrBegin and wwCellHdrEnd (the renderer marks the stencil, the cell programs write linear).
bool wwCellHdrActive();
//! lane VRS1: the linear frame's size while it is bound (0 x 0 otherwise)
void wwCellHdrFrameSize( int & w, int & h );
//! lane GATEFIX3: the linear frame's depth / stencil format (a GLenum) when `fbo` is its framebuffer, else 0. Its
//! texture samples stencil (TONEMAP1), so a framebuffer query reads its depth as 0 bits.
unsigned int wwCellHdrDepthFormat( unsigned int fbo );
//! Lane VOLAIR1 (gates only, WW_CELL_DRAW_PROBE): after a draw, logs which watched pixels it changed.
void wwCellDrawProbe( Scene * scene, const QString & what );
//! lane PERF1: the probe is watching (a caller builds its label only then).
bool wwCellDrawProbeOn();
/*! lane PERF1: the exposure and the bloom come from the linear frame (wwCellHdrEnd reads it back, a frame late by
 *  default); true when glview's own measure submission must run this frame instead: WW_IS_MEASURE=old or red, the
 *  WW_CELL_IS_DUMP gate, the document's first frame, or the last frame took no linear frame. */
bool wwCellImageSpaceMeasureByPass( Scene * scene );
//! One line for the notes: what the last frame did (or why it did not).
QString wwCellHdrEcho();
/*! lane PERF4: the linear frame's FARTHEST depth per pixel (max over its samples) as of now, in an R32F texture the
 *  size of the frame; 0 when there is no frame or its depth is a renderbuffer (WW_CELL_HDR_STEX_RED). The far cards
 *  read it to stop a fragment that every sample's depth test would refuse before it marches its sheets. */
unsigned int wwCellHdrDepthMax( Scene * scene );
//! lane CELLUNI6: a draw wrote the frame's depth after taking the depth max: the next reader makes it again
void wwCellHdrDepthMaxWrote();
/*! lane WATERUNDER4: the frame's depth per SAMPLE for one pass: binds its multisample depth texture in depth mode on
 *  `unit` and a framebuffer holding only the frame's colour (sampling an attached depth is a feedback loop). Returns the
 *  samples, 0 when there is one, the depth is a renderbuffer or no frame is bound; then nothing changed. */
int wwCellHdrDepthMsBegin( Scene * scene, int unit );
//! lane WATERUNDER4: after wwCellHdrDepthMsBegin returned > 0: stencil mode back, the frame's framebuffer bound again
void wwCellHdrDepthMsEnd( Scene * scene, int unit );

#endif
