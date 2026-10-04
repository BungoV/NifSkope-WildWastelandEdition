/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLAODECALGL_H
#define CELLAODECALGL_H

/* THE BAKED AO DECALS ON SCREEN (lane AODECAL1, 2026-10-04; src/cellaodecal.h is the set, src/aovolume.h the
 * volume). After the ambient obscurance's opaque pass (wwCellAoPass, step 1) every decal copy is drawn twice
 * against that pass's depth-stencil: its own triangles set a stencil bit where they are the visible surface (a
 * copy never darkens itself), then its footprint box's back faces multiply AO(m, n) into a full-size R32F target
 * cleared to 1 wherever the bit is clear; the bit is cleared for the next copy. Copies multiply (his call).
 * cellGiE (res/shaders/cell_lights.glsl) multiplies the probe term by the target's texel; nothing else.
 * Pins: WW_CELL_AODECAL_RED=add (the gate's red: AO - 1 summed instead of multiplied), WW_CELL_AODECAL_DUMP=<file>
 * (the target, the copies' ids, the opaque pass and the numbers; tests/spells/aodecal1_real.py). */

#include <QString>

#include <memory>

class Scene;
struct AoDecalSet;

//! the cell document's decal copies (null: none); the relight that built the copy-free grid publishes them
void wwCellAoDecalPublish( const void * nif, std::shared_ptr<const AoDecalSet> set );
bool wwCellAoDecalHas( const void * nif );
/*! called by wwCellAoPass after the opaque pass, its framebuffer still bound: gbufTex (view normal, linear depth),
 *  depthRb (its depth-stencil), W x H. Returns the R32F target's texture, 0 when no decal ran this frame. */
unsigned wwCellAoDecalRun( Scene * scene, unsigned gbufTex, unsigned depthRb, int W, int H );
QString wwCellAoDecalEcho();

#endif // CELLAODECALGL_H
