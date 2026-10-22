#ifndef CELLFXRES_H
#define CELLFXRES_H

#include <QString>
#include <QVector>

#include <string_view>

class Node;
class Scene;
class Shape;

/*! Lane FXRES1 (2026-10-22, notes/perfresearch1/FXRES1.md): SOFT EFFECTS AT A LOWER RESOLUTION IN THE CELL VIEW.
 *
 *  Shipped games draw their big soft effects (smoke, steam, fog cards) into a smaller target and composite it back
 *  with a depth-aware upsample (Cantlay, GPU Gems 3 ch. 23, off-screen particles; Jansen + Bavoil 2011; UE's
 *  separate translucency). Here: a Soft effect (BSEffectShaderProperty Soft, a soft depth, blended, no depth write,
 *  not a decal, not lit by the spot it stands in) large on screen leaves the sorted pass's order and draws, after
 *  the other sorted shapes and before the particles, into a layer of 1/2 the frame per axis. The layer's depth is
 *  the nearest opaque depth under each texel; the soft fade reads that depth. The layer keeps the draws' sum as an
 *  offset and a transmittance (each draw is dst = src x A + dst x B with B a function of its alpha), so the
 *  composite is exact at the layer's resolution: frame x T + C, by dual-source blending. AO, the opaque pass and
 *  the frame's size stay full resolution.
 *
 *  WW_CELL_FXRES=1 turns it on (OFF until bungo sees the pairs). WW_CELL_FXRES_SCALE=1|2|4 (pixels per layer texel,
 *  per axis; 2), WW_CELL_FXRES_MINPX (a draw's projected radius in pixels to leave its place; 32),
 *  WW_CELL_FXRES_MINAREA (the soft set's summed screen share to use the layer at all; 0.1), WW_CELL_FXRES_EDGE
 *  (the relative depth step of an edge in the upsample; 0.1). Reds: WW_CELL_FXRES_RED=full (the classification and
 *  census run, every draw stays in place: equals off), =quarter (1/4 per axis: must break the bar), =bilinear (the
 *  upsample ignores depth; information only). WW_CELL_FXRES_CENSUS=1: one line a frame. */

//! glscene.cpp drawDeferredShapes, after the sort: the soft draws the layer takes this pass (left out of the
//! in-place order; the first soft draw's scene, as the pass may hold several documents). Empty while the row is off, under the red full, or when the pass does not qualify.
QVector<Node *> wwCellFxResSplit( const QVector<Node *> & sorted );
//! glscene.cpp drawDeferredShapes, after the other sorted shapes: the layer drawn and composited once.
void wwCellFxResDraw( const QVector<Node *> & soft );
//! renderer.cpp: true while the layer draws (the cell effect program takes its fx variant)
bool wwCellFxResDrawing();
//! renderer.cpp, per draw while the layer draws: a draw the layer cannot hold exactly is masked there and drawn
//! in place after the composite
void wwCellFxResDrawHook( Shape * mesh, const std::string_view & program );
//! One line for the notes: what the last pass did, or why it did not.
QString wwCellFxResEcho();

#endif
