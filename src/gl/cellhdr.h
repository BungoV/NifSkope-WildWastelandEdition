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
//! One line for the notes: what the last frame did (or why it did not).
QString wwCellHdrEcho();

#endif
