#ifndef CELLHDR_H
#define CELLHDR_H

#include <QString>

class Scene;

/*! Lane HDR1 (2026-10-02, docs/PRTP_PLAN.md "one tone map"): THE CELL VIEW'S LINEAR FRAME.
 *
 *  The game draws its surfaces AND its effects into one linear (HDR) target and runs the imagespace (bloom,
 *  exposure, the curve, the grade, the LUT) once, on the sum. The cell view used to tone-map every fragment
 *  and blend effects over the result in display space: stacked additive haze cards each added their own
 *  tone-mapped value and turned the walkway's far door white.
 *
 *  While the cell's imagespace draws (Cell lights row, an interior with an IMGS), the main draw goes into a
 *  float target the size (and sample count) of the frame. The cell programs write linear light there
 *  (cellIsLinear in res/shaders/cell_lights.glsl); every other program writes as it always did and the
 *  stencil remembers which kind wrote a pixel last (1 a cell program or a cell effect, 2 anything else).
 *  Then one full-screen pass (cell_hdr.prog) draws the frame back: the imagespace over the 1s, the plain
 *  value over the 2s, nothing over the pixels no draw reached. Depth and stencil go back first, so what draws
 *  after the scene still depth-tests against it.
 *
 *  Part of the Cell lights row: no setting of its own. Harness only: WW_CELL_HDR_RED=perfrag keeps the old
 *  per-fragment tone map (the gate's red control). A pick, probe, measure or workspace frame never takes it. */

//! Before the main scene draw: binds the linear frame when this frame takes it; true when it did.
bool wwCellHdrBegin( Scene * scene );
//! After the main scene draw: the one tone map, back into the frame bound before wwCellHdrBegin.
void wwCellHdrEnd( Scene * scene );
//! True between wwCellHdrBegin and wwCellHdrEnd (the renderer marks the stencil, the cell programs write linear).
bool wwCellHdrActive();
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

#endif
