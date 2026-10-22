/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLREFLPROBE_H
#define CELLREFLPROBE_H

class Scene;

/*! THE PLACED REFLECTION PROBES (lane REFLPROBE1, notes/reflprobe1/RESEARCH.md; RDR2, The Division, Far Cry 4).
 *  Placed automatically: one probe per room of the probe GI's fine room grid (big rooms split into boxes of at most
 *  2048 units), else a 3 x 3 grid of 2048-unit cells round the camera at its height; plus RDR2's camera probe, which
 *  takes whatever the boxes leave. At most 32 placed probes are resident (the nearest).
 *  Captured once as a 64-pixel G-buffer cube (albedo; world normal + distance) from the static opaque shapes
 *  (alpha test kept, per-face culling), one face a frame for the placed queue and one for the camera probe.
 *  Relit by res/shaders/cell_reflrelight.frag with the cell's own light code (placed lights + shadow cubes, the
 *  probe GI and its sky visibility, the ambient, the sun x cascades) into an octahedral tile of an R11G11B10F atlas
 *  with box mips: a probe on capture, and again when the light changes (sun, colour, ambient, light count, GI).
 *  An env-mapped cell surface inside a probe's box takes the box-faded, parallax-corrected probe reflection in
 *  place of its material cube (res/shaders/cell_reflprobe.glsl), at lane CUBE1's strength; SSR stays on top.
 *  The row "Reflection probes" (WW/CellReflProbe; pin WW_CELL_REFLPROBE) ships OFF. Red
 *  WW_CELL_REFLPROBE_RED=fixed: placed, captured and relit, the material cube returned (must equal off);
 *  nosun|nolights|nogi|white strip the relight's terms. WW_CELL_REFLPROBE_ALL=1 captures and relights every probe in
 *  the frame it is placed (stills, gates). Probe 66 (WW_CELL_LIT_PROBE): the boxes' weight, the camera probe's
 *  share, the added light. stderr "reflprobe: ..." on change. */
void wwCellReflProbePass( Scene * scene, bool run );
bool wwCellReflProbeOn();
void wwCellReflProbeSetOn( bool on );
//! the draw setupProgram is about to bind: does its material read the greyscale palette (the sampler the atlas borrows)
void wwCellReflProbeNote( bool greyscalePalette );
//! called with every program setupProgram binds: the probes a cell-lit draw reads
void wwCellReflProbeDraw( Scene * scene, bool cellProgram );

#endif
