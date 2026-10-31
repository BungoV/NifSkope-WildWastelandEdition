#ifndef CELLFARINST_H
#define CELLFARINST_H

/* lane FARDRAW1 (2026-10-26): the far rings drawn instanced. Each ring bucket (a document BSTriShape with no
 * vertex rows, CellMesh::inst set) holds every base mesh ONCE plus a 64-byte row a placement; this module keeps
 * one GPU copy of each (VAO + vertex/index/row buffers per QOpenGLContext), culls the bucket's 4x4-cell tiles
 * against the camera on the CPU and draws the survivors in one multi-draw-indirect call.
 *   WW_CELL_FARINST_RED=nocull   every tile drawn (the gate for "rows drawn == rows built")
 *   WW_CELL_FARINST_CENSUS=1     a "farinst:" line on stderr whenever a frame's totals change */

#include "gl/glcontext.hpp"
#include "gl/farswayslice.h"

struct CellMeshInst;
class Scene;
class Transform;

namespace CellFarInst
{
//! The main pass: binds the copy, sets `farInst` on prog, draws, clears it. False = nothing drawn.
bool draw( NifSkopeOpenGLContext * r, Scene * scene, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const Transform & viewTrans );
//! A shadow cascade: clipFromModel (column-major) culls the tiles by the cascade's box; prog's modelViewMatrix is set
//! by the caller.
//! lane SHADOWSHIMMER4: with `slice` (+ modelView, column-major) only the tiles of `part` (gl/farswayslice.h: Still =
//! outside the cascade's view slice, Sway = meeting it) draw; null = every tile, as before.
bool drawCascade( NifSkopeOpenGLContext * r, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const float clipFromModel[16], const float * modelView = nullptr, const WwFarSwaySlice::Slice * slice = nullptr,
	int part = 3 );
//! lane SHADOWSHIMMER4: the tiles (of those a streamed near cell leaves drawn) meeting the slice; their indices into `hash`
int sliceTiles( const CellMeshInst & d, const float modelView[16], const WwFarSwaySlice::Slice & slice, quint64 & hash );

/* lane SKYSHARDS3 (2026-10-30): the far ring never draws with a bindless twin (fo4_cellbl / fo4_cellcsmbl). The twin
 * reads its per-draw row (matrices + texture handles) at wwDrawBase + gl_BaseInstance (ww_bindless.glsl), and a far
 * command's base instance is its first PLACEMENT row: every command past row 0 read another draw's row, rewritten as
 * the ring advances (Concord's faceted, ground-textured shards in the sky, a new shape every frame), or a row past the
 * ring's end, or a handle a reopen had made non-resident. The plain fo4_cell / fo4_cellcsm program binds its own.
 * Red WW_CELL_FARINST_RED=blrow: the twin as before (live). */
bool bindlessTwinAllowed();
//! The rows a ring's commands read under a program (twin of ww_bindless.glsl's wwDrawIdx for an instanced draw)
struct RowAudit
{
	qint64 cmds = 0;        //!< commands
	qint64 foreign = 0;     //!< commands reading a row that is not the draw's own (bindless twin, base instance > 0)
	qint64 pastRing = 0;    //!< of those, past the ring's last row from any draw's row
	quint32 maxOffset = 0;  //!< the largest row offset read
};
RowAudit rowAudit( const CellMeshInst & d, bool bindlessProgram, quint32 ringRows = 65536 );
}

#endif
