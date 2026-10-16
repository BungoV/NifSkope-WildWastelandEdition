/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLAODECALGL_H
#define CELLAODECALGL_H

/* Draws the baked AO decal copies into a full-size R32F multiplier target after the AO opaque pass;
 * cellGiE multiplies the probe term by it. WW_CELL_AODECAL_RED=add, WW_CELL_AODECAL_DUMP=<file>: debug switches. */

#include <QString>

#include <memory>

class Scene;
struct AoDecalSet;

//! the cell document's decal copies (null: none); the relight that built the copy-free grid publishes them
void wwCellAoDecalPublish( const void * nif, std::shared_ptr<const AoDecalSet> set );
bool wwCellAoDecalHas( const void * nif );
//! the document has decal copies AND the AO decals row is on (the shared opaque pass is then needed)
bool wwCellAoDecalWanted( const void * nif );
/*! called by wwCellAoPass after the opaque pass, its framebuffer still bound: gbufTex (view normal, linear depth),
 *  depthRb (its depth-stencil), W x H. Returns the R32F target's texture, 0 when no decal ran this frame. */
unsigned wwCellAoDecalRun( Scene * scene, unsigned gbufTex, unsigned depthRb, int W, int H );
QString wwCellAoDecalEcho();

#endif // CELLAODECALGL_H
