#ifndef WW_CELLFARGROUND_H
#define WW_CELLFARGROUND_H

/* lane FARDRAW1 (bungo 10-26): the far ring 0's ground, drawn live per pixel from the bake's .lodl weights (the
 * splat) with the .lodg/.lodd decals on top, instead of the baked sheet. Two decal options behind one Show-menu row:
 *   Baked                   -- the baked sheet, as before (the default; the twin programs are never chosen)
 *   Live, shader decals     -- each pixel looks its decals up in a 512-unit bin list (fo4_cellfgl / fo4_cellcsmfgl)
 *   Live, cached pages      -- lane FARPAGES1: the layers + decals painted once into a BC1 page a cell, in two pools by
 *                              distance; a few pages a frame, nearest first; a cell with no page shows the sheet and fades
 *                              to its page (fo4_cellfgp / fo4_cellcsmfgp). "Far ground pages" Low/Medium/High = near/far
 *                              sides 256/128, 512/256, 1024/512 px a cell, near split k = 2, 1, 0.5 screen pixels a texel.
 * Pins WW_CELL_FARGROUND=baked|lookup|cached, WW_CELL_FARGROUND_QUALITY=low|medium|high (ultra reads as high); red
 * WW_CELL_FARGROUND_RED=nodecals (no decals) | baked (the twins never chosen) | oldpages (FARDRAW1's decal-only pages
 * over the live splat, fo4_cellfgc) | nocompress (RGBA8 pools). All live.
 * Not covered: normals stay the sheet's, no .loda AO, the PBR route and the streamed ring 0 keep the sheet. */

#include "gl/glcontext.hpp"

#include <QString>
#include <QStringList>

class EsmWorld;
class Scene;

namespace CellFarGround
{
int mode();				// 0 baked, 1 lookup, 2 cached
void setMode( int m );
bool modePinned();
int quality();			// 0..2
void setQuality( int q );
bool qualityPinned();

//! At the far field's append, after ring 0's terrain: blocks [blockFirst, blockLast) are its shapes, the ring
//! rect and its cut in cells. Reads the grid and the decal records (CPU); pictures load on first use.
void prepare( const void * nif, const EsmWorld & world, const QString & lodlPath, int blockFirst, int blockLast,
	int rx0, int ry0, int rx1, int ry1, int cx0, int cy0, int cx1, int cy1, QStringList * notes );
void forget( const void * nif );
//! The twin a ring-0 shape draws with: 0 none (the baked sheet), 1 lookup, 2 old pages (red), 3 cached pages.
int twinFor( const void * nif, int block );
//! True for a ring-0 terrain shape, whatever the option (the GPU zone far-ground-draw).
bool ring0( const void * nif, int block );
//! True while cached pages are still painting or fading in (the view keeps drawing frames).
bool wantsFrames( Scene * scene );
//! Per frame, before the draws: uploads, stamps and frees by mode (a GL context is current).
void pass( Scene * scene );
//! A twin program's textures and uniforms (every draw of one).
void uniforms( Scene * scene, NifSkopeOpenGLContext::Program * prog );
}

#endif
