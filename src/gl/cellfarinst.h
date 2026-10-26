#ifndef CELLFARINST_H
#define CELLFARINST_H

/* lane FARDRAW1 (2026-10-26): the far rings drawn instanced. Each ring bucket (a document BSTriShape with no
 * vertex rows, CellMesh::inst set) holds every base mesh ONCE plus a 64-byte row a placement; this module keeps
 * one GPU copy of each (VAO + vertex/index/row buffers per QOpenGLContext), culls the bucket's 4x4-cell tiles
 * against the camera on the CPU and draws the survivors in one multi-draw-indirect call.
 *   WW_CELL_FARINST_RED=nocull   every tile drawn (the gate for "rows drawn == rows built")
 *   WW_CELL_FARINST_CENSUS=1     a "farinst:" line on stderr whenever a frame's totals change */

#include "gl/glcontext.hpp"

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
bool drawCascade( NifSkopeOpenGLContext * r, const CellMeshInst & d, NifSkopeOpenGLContext::Program * prog,
	const float clipFromModel[16] );
}

#endif
