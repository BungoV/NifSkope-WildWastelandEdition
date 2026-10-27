#ifndef CELLOCEAN_H
#define CELLOCEAN_H

#include <QString>

class Scene;

/*! Lane WATERLOOK1 (2026-10-27): THE WIND'S WAVE SPECTRUM ON THE BAKED WATER (the Ocean waves row).
 *
 *  Each frame, while the row is on and the cell draws water, the spectrum (src/gl/oceanspectrum.h: JONSWAP with
 *  directional spread, the sea's swell, a lake's long ripples, the Cox-Munk cap) is evolved on the GPU and transformed
 *  by a fragment FFT (Tessendorf 2001; no compute in this GL 4.1 context): 1 spectrum pass, 14 butterfly passes, one
 *  layered pass into two 6-layer arrays (sea 0-2, fresh water 3-5) and 7 layered mip passes. 0.14 ms at 1080p on an
 *  RTX 5070 Ti (notes/waterlook1/tools/ocean_gl.py). fo4_waterlook.prog reads the arrays (res/shaders/water_ocean.glsl)
 *  for the normals, the slope variance (into the glare's roughness) and the whitecaps on every baked body; placed and
 *  interior water keep today's look. Wind and swell are the shore foam's (the weather's wind byte).
 *
 *  Row "Ocean waves" (WW/CellWaterOcean), pin WW_CELL_WATER_OCEAN=0|1. Off: no GL call, the water draws as before.
 *  Red WW_CELL_WATER_OCEAN_RED = old (today's normals under the row) | tile (plain repeats) | nopatch (no wind patches)
 *  | novar (no lost slope variance) | nochop (no chop in the slopes or the whitecaps).
 *  Probes WW_CELL_WATER_PROBE=39 (the spectrum's slope, its weight) and 40 (whitecaps, lost slope rms, caustic factor). */

//! Build this frame's spectrum (before the reflections pass: the water's ray pass reads it too).
void wwCellOceanPass( Scene * scene, bool cellPasses );
//! With fo4_waterlook.prog bound for a body whose WATR wind (first noise layer) points at windDeg (the compass, 0 = +Y).
void wwCellOceanUniforms( Scene * scene, float windDeg );
//! One telemetry part: what the last frame built (empty while the row is off).
QString wwCellOceanEcho();

#endif
