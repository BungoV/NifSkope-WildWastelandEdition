#ifndef CELLFARINST_H
#define CELLFARINST_H

/* lane FARDRAW1 (2026-10-26): the far rings drawn instanced. Each ring bucket (a document BSTriShape with no
 * vertex rows, CellMesh::inst set) holds every base mesh ONCE plus a 64-byte row a placement; this module keeps
 * one GPU copy of each (VAO + vertex/index/row buffers per QOpenGLContext), culls the bucket's N x N-cell tiles (gl/fartile.h)
 * against the camera on the CPU and draws the survivors in one multi-draw-indirect call.
 *   WW_CELL_FARINST_RED=nocull   every tile drawn (the gate for "rows drawn == rows built")
 *   WW_CELL_FARINST_CENSUS=1     a "farinst:" line on stderr whenever a frame's totals change */

#include "gl/glcontext.hpp"
#include "features/far-lod/farswayslice.h"

struct CellMeshInst;
class QString;
class Scene;
class Transform;

namespace CellFarInst
{
//! The main pass: binds the copy, sets `farInst` on prog, draws, clears it. False = nothing drawn.
//! lane FARLOOK1101: splatPass 0 = every row as before, 1 = the opaque pass of a splat twin (rows at or over S),
//! 2 = the blended splat pass (rows under S, drawn widened at alpha 1 / (kx * ky), no depth write).
bool draw( NifSkopeOpenGLContext * r, Scene * scene, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const Transform & viewTrans, int splatPass = 0 );

/* lane FARLOOK1101 (FARSHIMMER2 = FARSPLAT1 + FARTHIN1, 2026-11-01): with the TAA off a far row under ~1/8 px^2 blinks
 * on a moving camera (FARTOGGLE4: 548 of 14,573 Concord rows). WW_CELL_FARSPLAT=<S px> (1/on = 2.0, the census pick,
 * tests/features/far-lod/farshimmer/splat_rule.py) draws every row whose projected box is under S px on an axis in the second pass,
 * widened to S about its centre at alpha 1 / (kx * ky): ink-true by construction, 0 blinkers in the census. Programs
 * fo4_cellfs / fo4_cellcsmfs (twins: the default binaries stay). Unset/0 = off (default: waits on bungo's eye) = the
 * old rule. Red WW_CELL_FARSPLAT_RED=noalpha: the splat drawn opaque (alpha 1, the census red: ink x3.8). */
float splatPx();
bool splatRedNoAlpha();
//! A shadow cascade: clipFromModel (column-major) culls the tiles by the cascade's box; prog's modelViewMatrix is set
//! by the caller.
//! lane SHADOWSHIMMER4: with `slice` (+ modelView, column-major) only the tiles of `part` (gl/farswayslice.h: Still =
//! outside the cascade's view slice, Sway = meeting it) draw; null = every tile, as before.
bool drawCascade( NifSkopeOpenGLContext * r, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const float clipFromModel[16], const float * modelView = nullptr, const WwFarSwaySlice::Slice * slice = nullptr,
	int part = 3 );
//! lane SHADOWSHIMMER4: the tiles (of those a streamed near cell leaves drawn) meeting the slice; their indices into `hash`
int sliceTiles( const CellMeshInst & d, const float modelView[16], const WwFarSwaySlice::Slice & slice, quint64 & hash );
/*! lane PERFFIX2 b (gl/sunshadowdyn.h): the clip x / y bounds (x0, y0, x1, y1) of the tiles drawCascade draws with
 *  clipFromModel (slice + modelView: only those meeting the slice; null: every tile), each tile's sphere grown by `grow`
 *  model units (the far sway's largest offset). The tile sphere is the one drawCascade culls with. False: no tile. */
bool tileBounds( const CellMeshInst & d, const float clipFromModel[16], const float * modelView,
	const WwFarSwaySlice::Slice * slice, float grow, float out[4] );

/* lane SKYSHARDS3 (2026-10-30): the far ring never draws with a bindless twin (fo4_cellbl / fo4_cellcsmbl). The twin
 * reads its per-draw row (matrices + texture handles) at wwDrawBase + gl_BaseInstance (ww_bindless.glsl), and a far
 * command's base instance is its first PLACEMENT row: every command past row 0 read another draw's row, rewritten as
 * the ring advances (Concord's faceted, ground-textured shards in the sky, a new shape every frame), or a row past the
 * ring's end, or a handle a reopen had made non-resident. The plain fo4_cell / fo4_cellcsm program binds its own.
 * Red WW_CELL_FARINST_RED=blrow: the twin as before (live).
 * lane FARLOOK1101 (FARINSTBL1): WW_CELL_FARINST_BL=1 lets the twin back with the row in the uniform alone
 * (ww_bindless.glsl wwBlUniformRow: wwDrawIdx = wwDrawBase, the shape's own committed row, for every command). Off
 * by default (the old rule: own program); the red wins over it. */
bool bindlessTwinAllowed();
bool bindlessUniformRow();
/*! MODULES1 10b-2: far draws whose baked AO sits in the vertex colour (fo4_default.vert farVcAo). vcAoType: 0 none,
 *  1 the flat far copy ("FarLOD r<i> ..."), 2 the far ground ("FarLOD terrain r<i> ..."), from the shape's name; 0
 *  under a WW_LODL_CHANNEL view (the colours are data then). vcAoUniform sets the draw's type (3 = instanced rows
 *  without the stream) with the live maxima WW_LODL_AO_MAX / WW_LODL_SKY_MAX and the red WW_LODL_AO_RED=albedo. */
int vcAoType( const QString & shapeName );
void vcAoUniform( NifSkopeOpenGLContext::Program * prog, int type );
//! The rows a ring's commands read under a program (twin of ww_bindless.glsl's wwDrawIdx for an instanced draw)
struct RowAudit
{
	qint64 cmds = 0;        //!< commands
	qint64 foreign = 0;     //!< commands reading a row that is not the draw's own (bindless twin, base instance > 0)
	qint64 pastRing = 0;    //!< of those, past the ring's last row from any draw's row
	quint32 maxOffset = 0;  //!< the largest row offset read
};
RowAudit rowAudit( const CellMeshInst & d, bool bindlessProgram, quint32 ringRows = 65536, bool uniformRow = false );
}

#endif
