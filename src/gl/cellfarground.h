#ifndef WW_CELLFARGROUND_H
#define WW_CELLFARGROUND_H

/* lane FARDRAW1 (bungo 10-26): the far ring 0's ground, drawn live per pixel from the bake's .lodl weights (the
 * splat) with the .lodg/.lodd decals on top, instead of the baked sheet. Two decal options behind one Show-menu row:
 *   Baked                   -- the baked sheet, as before (the default; the twin programs are never chosen)
 *   Live, shader decals     -- each pixel looks its decals up in a 512-unit bin list (fo4_cellfgl / fo4_cellcsmfgl)
 *   Live, cached pages      -- the decals stamped once into a page a cell, sharpness by the "Far ground pages" preset
 *                              (Low/Medium/High/Ultra = 256/512/1024/2048 px a cell) (fo4_cellfgc / fo4_cellcsmfgc)
 * Pins WW_CELL_FARGROUND=baked|lookup|cached, WW_CELL_FARGROUND_QUALITY=low|medium|high|ultra; red
 * WW_CELL_FARGROUND_RED=nodecals (the live splat alone) | baked (the twins never chosen). All live.
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
int quality();			// 0..3
void setQuality( int q );
bool qualityPinned();

//! At the far field's append, after ring 0's terrain: blocks [blockFirst, blockLast) are its shapes, the ring
//! rect and its cut in cells. Reads the grid and the decal records (CPU); pictures load on first use.
void prepare( const void * nif, const EsmWorld & world, const QString & lodlPath, int blockFirst, int blockLast,
	int rx0, int ry0, int rx1, int ry1, int cx0, int cy0, int cx1, int cy1, QStringList * notes );
void forget( const void * nif );
//! The twin a ring-0 shape draws with: 0 none (the baked sheet), 1 lookup, 2 cached.
int twinFor( const void * nif, int block );
//! Per frame, before the draws: uploads, stamps and frees by mode (a GL context is current).
void pass( Scene * scene );
//! A twin program's textures and uniforms (every draw of one).
void uniforms( Scene * scene, NifSkopeOpenGLContext::Program * prog );
}

#endif
