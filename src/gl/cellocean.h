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
 *  Step 2 (displacement near): the final pass also writes a third array (heights + chop, RGBA16F, unit 70), and the
 *  water then draws with fo4_waterlookd.prog: fo4_default.vert, a split by fo4_waterdisp.tesc (crack-free: an edge's
 *  level is the edge's alone; about 8 px, at most 64 times a 4096 u quad, none past 8192 u) and fo4_waterdisp.tese
 *  moving each vertex by the same cascades the normals read, full to one cell, none past two. Its shapes draw as
 *  GL_PATCHES (Program::patches, triMode). A refused link falls back to fo4_waterlook.prog.
 *  Gates: notes/waterlook1/tools/tess_gl.py (cracks, winding, fade, 4 std( z ) vs Hs, reds).
 *  Step 3 (water_ripple.glsl): ring trains (0.3 m, 0.8 m) run outward from whatever stands in the water, as strong as
 *  the unresolved short waves there, fading over 3 wavelengths; the contact is the face behind the pixel (the depth's
 *  tangent plane met with the water), so every drawn object counts; a jump in the depth or a face seen edge-on draws
 *  none; a train finer than the pixel goes to the glare's roughness. Gates: notes/waterlook1/tools/ripple_gl.py.
 *  Step 4 (water_sss.glsl): sunlight through the crests (the body colour, forward lobe g 0.5, faces tilted to the eye);
 *  a pixel takes the expected facing part over its resolved slope and the lost variance, so far water shows the
 *  spectrum's mean glow with no shimmer. Gates: notes/waterlook1/tools/sss_gl.py.
 *  Step 5 (water_flowwaves.glsl): on the baked river flow (WATERFLOWBAKE1) the waves' slopes travel downstream, two
 *  reads half a 4 s period apart, each fading out at its own restart; the mix is renormalised by the slope field's
 *  correlation at the reads' separation, computed from the spectrum (oceanspectrum.h slopeCorr), so the water never
 *  breathes. The flow is bilinear between the bake's texels. Still water reads one sample (today's waves). The
 *  vertices (step 2) are not carried: fresh-water heights are centimetres. Gates: notes/waterlook1/tools/flow_gl.py.
 *
 *  Row "Ocean waves" (WW/CellWaterOcean), pin WW_CELL_WATER_OCEAN=0|1. Off: no GL call, the water draws as before.
 *  Red WW_CELL_WATER_OCEAN_RED = old (today's normals under the row) | tile (plain repeats) | nopatch (no wind patches)
 *  | novar (no lost slope variance, so no rings either) | nochop (no chop in the slopes, the whitecaps or the vertices)
 *  | flat (step 2: no displacement, fo4_waterlook.prog) | norip (step 3: no contact rings) | noglow (step 4: no crest
 *  glow) | noflow (step 5: the waves stay put on a flowing river).
 *  Probes WW_CELL_WATER_PROBE=39 (the spectrum's slope, its weight) and 40 (whitecaps, lost slope rms, caustic factor). */

//! Build this frame's spectrum (before the reflections pass: the water's ray pass reads it too).
void wwCellOceanPass( Scene * scene, bool cellPasses );
//! With fo4_waterlook.prog bound for a body whose WATR wind (first noise layer) points at windDeg (the compass, 0 = +Y).
void wwCellOceanUniforms( Scene * scene, float windDeg );
//! One telemetry part: what the last frame built (empty while the row is off).
QString wwCellOceanEcho();
//! Step 2: this frame's spectrum is built and the red is not flat (the water draws displaced, fo4_waterlookd.prog).
bool wwCellOceanDispOn();

#endif
