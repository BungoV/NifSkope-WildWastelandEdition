#ifndef CELLCULL_H
#define CELLCULL_H

#include "data/niftypes.h"

#include <QString>

#include <cstdint>
#include <vector>

class Scene;
class Shape;

/*! Lane SUNCELL1 (2026-10-05): GAME-STYLE CULLING IN THE CELL VIEW.
 *
 *  The cell view welds every placement of one material into a few big shapes (src/cellview.cpp), so a
 *  whole-shape test culls nothing. The writer remembers where each placement's triangles start in its shape
 *  (a RUN) and the run's bounding sphere, local to the shape; the renderer then draws only the runs that touch
 *  the camera's frustum (the side planes, as the game's per-object test), with byte offsets into the shape's
 *  element buffer, the way the edit-mode hidden triangles draw.
 *
 *  THE SHADOW RULE: the camera's cull never removes a caster. The sun's cascades draw their casters with their
 *  own cull, each cascade against its own light-space window (gl/sunshadow.cpp); the cell lights' cubes draw
 *  every caster (they never ask this table).
 *
 *  Not modelled: the game's previs / occlusion. The cell view has no PREVIS data read yet (proposal, STATUS.md).
 *
 *  Row: Culling (cell workspace; ships OFF). Pins (harness only):
 *    WW_CELL_CULL=0|1             the row, pinned
 *    WW_CELL_CULL_RED=casters     RED: the cascades' casters culled with the CAMERA's frustum (shadows of
 *                                 off-screen casters vanish; the cull-on == cull-off gate must fail)
 *  Telemetry (stderr, once per change): "cell cull: camera shapes=.. runs=.. drawn=.. culled=.. calls=.. tris=../..
 *  | casters c0 drawn/culled .. c1 .. c2 ..". */
struct WwCullRun
{
	std::uint32_t first = 0, count = 0;	//!< triangles [first, first + count) of the shape
	Vector3 center;				//!< bounding sphere, the shape's own space
	float radius = 0.0f;
};

//! A new scene for this document: its table is emptied.
void wwCellCullBegin( const void * nif );
//! The written shape `block` draws in these runs (in triangle order, covering every triangle).
void wwCellCullShape( const void * nif, int block, std::vector<WwCullRun> && runs );
//! The shape's runs, or null (not a cell shape, or one run only).
const std::vector<WwCullRun> * wwCellCullRuns( const void * nif, int block );

bool wwCellCullOn();
void wwCellCullSetOn( bool on );
//! 1 = the red control `casters`
int wwCellCullRed();

/*! The camera's visible ranges of `block`'s first `numTris` triangles, adjacent runs merged, as
 *  (first, count) pairs into `out`. False = draw the shape whole (culling off, no runs, a probe pass). */
bool wwCellCullCamera( Scene * scene, const Shape * sh, int block, std::int64_t numTris,
	std::vector<std::uint32_t> & out );
/*! The caster pass: the runs of `block` inside one cascade's window. `clipFromView` is that cascade's
 *  column-major view -> clip matrix (x, y in [-1, 1] inside). With the red control `casters` the CAMERA's
 *  frustum is used instead. False = draw whole. */
bool wwCellCullCaster( Scene * scene, const Shape * sh, int block, int cascade, const float * clipFromView,
	std::vector<std::uint32_t> & out );

//! Once per frame, before the passes: prints the last frame's counts when they changed.
void wwCellCullFrame();
//! The last frame's counts, one line.
QString wwCellCullSummary();

#endif
