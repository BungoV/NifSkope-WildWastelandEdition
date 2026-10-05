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
 *  PREVIS AND OCCLUSION (the Previs row, below): what the plugin says about visibility, applied the game's way to
 *  the CAMERA pass only -- the occlusion planes and boxes (PlaneMarker primitives) hide what lies wholly behind
 *  them, and indoors the rooms and portals (RoomMarker boxes, PortalMarker planes, XPOD / XLRM links) hide a room
 *  the camera's room cannot reach through a portal in the frustum. Casters always cast: no shadow pass asks it.
 *  The worldspace's precombined / previs data is READ (cellview.cpp: CELL RVIS, VISI, PCMB, XCRI, XPRI and the
 *  previs file's header) and reported; the previs file's body is an Umbra tome whose visibility query is not
 *  decoded, so the tome itself culls nothing here.
 *
 *  Row: Culling (cell workspace; ships OFF). Pins (harness only):
 *    WW_CELL_CULL=0|1             the row, pinned
 *    WW_CELL_CULL_RED=casters     RED: the cascades' casters culled with the CAMERA's frustum (shadows of
 *                                 off-screen casters vanish; the cull-on == cull-off gate must fail)
 *  Telemetry (stderr, once per change): "cell cull: camera shapes=.. runs=.. drawn=.. culled=.. calls=.. tris=../..
 *  | casters c0 drawn/culled .. c1 .. c2 ..".
 *
 *  Row: Previs (cell workspace; ships OFF). Pins (harness only):
 *    WW_CELL_PREVIS=0|1           the row, pinned
 *    WW_CELL_PREVIS_RED=casters   RED: the occlusion and the rooms applied to the sun cascades' casters too
 *                                 (a shadow cast from behind an occluder vanishes; the casters-kept gate must fail)
 *  Telemetry: "cell previs cull: on|off occluders=.. rooms=.. roomsSeen=.. portalsPassed=.. occluded=.. roomCulled=..
 *  casterHidden=.." once per change. */
struct WwCullRun
{
	std::uint32_t first = 0, count = 0;	//!< triangles [first, first + count) of the shape
	Vector3 center;				//!< bounding sphere, the shape's own space
	float radius = 0.0f;
};

/*! lane SUNCELL1: one primitive of the previs scene, in the cell document's space: centre, unit axes, half
 *  extents. `type` 1 box, 3 plane (the thinnest axis is the normal). A portal names its rooms (`from`, `to`, 0 =
 *  outside); a room names the rooms linked to it. */
struct WwPrevisBox
{
	Vector3 c;
	Vector3 ax[3];
	float half[3] = { 0, 0, 0 };
	int type = 1;
	std::uint32_t ref = 0, from = 0, to = 0;
	std::vector<std::uint32_t> linked;
};
struct WwPrevisScene
{
	std::vector<WwPrevisBox> occluders, rooms, portals;
};
//! The document's previs scene (replaced whole; wwCellCullBegin forgets it).
void wwCellPrevisSet( const void * nif, WwPrevisScene && sc );
bool wwCellPrevisOn();
void wwCellPrevisSetOn( bool on );
//! 1 = the red control `casters`
int wwCellPrevisRed();

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
