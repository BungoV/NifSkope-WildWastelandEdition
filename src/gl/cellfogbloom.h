#ifndef CELLFOGBLOOM_H
#define CELLFOGBLOOM_H

#include <QString>

class Renderer;
class Scene;

/*! Lane FOGBLOOM1 (2026-10-22, from RDR2's bloom-intensity target): a per-pixel bloom strength for the cell view.
 *  At the tone map (cellhdr.cpp) a full-screen pass writes an R8 target the size and sample count of the linear frame:
 *  gain = 1 + k f (1 - T) (1 + b translucent), with T the air's transmittance along the view ray to the pixel
 *  (lookdev_fog.glsl, the froxel volume), f a smoothstep over the ray's mean extinction from mist (V 2 km) to fog
 *  (V 1 km), and "translucent" the samples a blended linear-light draw (effects, glass) wrote last (stencil bit 0x04,
 *  set by renderer.cpp only while this is on). The tone map multiplies both blooms (Vanilla and FO4CS) by the gain.
 *  Clear air writes exactly 0, so the frame is the same bits as without the lane. Research: notes/fogbloom1/RESEARCH.md.
 *
 *  Row: Cell workspace, Show > Bloom > Fog bloom (WW/CellFogBloom, off).
 *  Pins: WW_CELL_FOGBLOOM=0|1 (a harness without it runs off); red WW_CELL_FOGBLOOM_RED=flat: the uniform bloom
 *  (no pass, no marks); WW_CELL_FOGBLOOM_TIME=1 prints the pass's GPU time (timestamp pairs, read a frame late). */

bool wwCellFogBloomOn();
void wwCellFogBloomSetOn( bool on );
bool wwCellFogBloomPinned();
//! True while the frame's blended linear draws should set stencil bit 0x04 (on and not red flat).
bool wwCellFogBloomMarks();
/*! In wwCellHdrEnd, before the tone map: the intensity target from the linear frame's fbo, its multisampled
 *  depth / stencil ds (the same format both sides; a texture when dsTexture, else a renderbuffer). True when the
 *  tone map should read it. */
bool wwCellFogBloomRun( Scene * scene, unsigned hdrFbo, unsigned ds, int w, int h, int samples, unsigned dsFormat, bool dsTexture );
//! The tone map's uniforms (cell_hdr.prog current), the target on `unit`; cellFogBloomOn false when the run did not.
//! knownActive: the active unit (GL_TEXTURE0 + n) the caller knows, -1 = ask (lane CELLUNI5)
void wwCellFogBloomFinal( Renderer * r, int unit, bool ran, int knownActive = -1 );
void wwCellFogBloomDone( Renderer * r, int unit, int knownActive = -1 );
QString wwCellFogBloomEcho();

#endif
