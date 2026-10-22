#ifndef CELLVRS_H
#define CELLVRS_H

#include <QString>

class Scene;

/*! Lane VRS1 (2026-10-22, notes/perfresearch1/VRS1.md): VARIABLE RATE SHADING IN THE CELL VIEW.
 *
 *  Shipped games (Gears 5 / Gears Tactics Tier 2 VRS, Wolfenstein: Youngblood's adaptive shading, AMD's
 *  FidelityFX-VariableShading) shade flat, low-contrast parts of the picture once per 2x2 pixels instead of per
 *  pixel. The rate per 16x16 tile comes from the PREVIOUS frame's finished picture: the largest step between
 *  neighbouring pixels across the tile, horizontally and vertically apart (FidelityFX's method, own GLSL), the
 *  allowance raised where the camera moves fast (the temporal AA's vectors). Coverage, depth and the frame's size
 *  stay per sample: only the number of fragment-shader runs drops (GL_NV_shading_rate_image).
 *
 *  Applied to the opaque pass, and to the sorted pass with a second, stricter image (Gears' "conservative" image).
 *  Never to shadows, the obscurance (full resolution), the sky, effects, far cards, the tone map, post or overlays.
 *  Alpha-tested draws shade 1x1 (a coarse discard drops whole blocks).
 *
 *  WW_CELL_VRS=1 turns it on (OFF by default until bungo sees the pair). WW_CELL_VRS_THRESH / _THRESH_SORTED (the
 *  step allowance, display units 0..1), WW_CELL_VRS_MAX=2x2|4x4, WW_CELL_VRS_MOTION (the motion weight).
 *  Reds: WW_CELL_VRS_RED=1x1 (the whole machinery, every rate 1x1: must equal off), =4x4 (every tile 4x4: must
 *  break the bar), =noalpha (alpha-tested draws keep the coarse rate). Gates: WW_CELL_VRS_CENSUS=1 (a line of rate
 *  shares a frame), WW_CELL_VRS_DUMP=<file> (the opaque image as a PGM, <file>.sorted.pgm the sorted one). */

//! glscene.cpp, around the opaque (sorted false) and the sorted pass: on binds and enables the rate image when
//! this frame has one; off disables it. No GL call while the row is off.
void wwCellVrsPass( Scene * scene, bool sorted, bool on );
//! True while a pass shades through the rate image (the per-draw hook below is wanted).
bool wwCellVrsActive();
//! renderer.cpp, per draw while active: an alpha-tested draw shades 1x1.
void wwCellVrsDraw( bool alphaTested );
//! cellhdr.cpp, after the tone map: the next frame's rate images from this frame's picture (bound as prevDraw).
//! vp: the frame's viewport, restored after. No GL call while the row is off.
void wwCellVrsBuild( Scene * scene, int prevDraw, int prevRead, int W, int H, const int vp[4] );
//! The row (WW_CELL_VRS=1 and not refused); cellhdr reads the viewport for the build only then.
bool wwCellVrsWanted();
//! One line for the notes: what the last frame did, or why it did not.
QString wwCellVrsEcho();

#endif
