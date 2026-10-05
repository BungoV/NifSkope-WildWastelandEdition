#ifndef CELLCULL_H
#define CELLCULL_H

#include "data/niftypes.h"

#include <QByteArray>
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
 *  previs file's header) and reported; the previs file's body is an Umbra tome, decoded by lane UMBRA1, whose
 *  visibility query culls the camera pass for the runs it governs (wwCellUmbraSet, below).
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
	std::uint32_t ref = 0;			//!< lane UMBRA1: the placed reference's form id (0 = none)
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

/*! Lane UMBRA1 (2026-10-05): THE PREVIS TOME CULLS THE CAMERA PASS (src/gl/cellumbra.h). One loaded previs block:
 *  its .uvd bytes and the precombined refs of its loaded cells (XCRI: ref form id, and the 0xFD id's cell code
 *  (x mod 32) << 19 | (y mod 32) << 14 of the cell that lists it; 0 for an interior).
 *
 *  Which runs the tome governs: a run whose ref is a tome object id (the full id, else its low 24 bits when that is
 *  unique) is drawn when that object is visible; a run whose ref went into a precombined mesh is drawn when any
 *  visible 0xFD object of its cell code overlaps the run's world sphere. Every other run (dynamic refs, LAND, a block
 *  whose tome is missing, failed to decode, or holds no start cell for this camera) keeps the frustum and the
 *  occluder / room path. Casters always cast: no shadow pass asks the tome.
 *
 *  Rides the Previs row (ships OFF). Pins (harness only):
 *    WW_CELL_UMBRA=0              the tome path off (the occluder / room path culls every run) -- gate 5
 *    WW_CELL_UMBRA_RED=casters    RED: the tome's hidden runs dropped from the sun cascades too (the casters-kept
 *                                 gate must fail)
 *    WW_CELL_UMBRA_DUMP=<dir>     tome_<block>.txt (structure, the Python `dump` twin) once per build, and
 *                                 query_<block>.txt (camera, start cells, cell coverage hashes, visible objects) on
 *                                 every camera change; the query runs while this is set even with the row off
 *    WW_CELL_UMBRA_DEPTH=1        with _DUMP: depth.txt, world points of a culling-off depth readback (gate 2)
 *  Telemetry: "cell umbra: on|off blocks=.. decoded=.. queried=.. noStart=.. cells=.. visible=../.. governed=..
 *  (uid .., combined ..) tomeCulled=.. ungoverned=.." once per change; "cell umbra tome <block>: decoded|FAILED"
 *  once per build. */
struct WwUmbraBlockIn
{
	std::uint32_t block = 0;	//!< the RVIS form
	QByteArray tome;		//!< the .uvd file
	std::vector<std::pair<std::uint32_t, std::uint32_t>> combined;	//!< (XCRI ref form id, 0xFD cell code)
};
void wwCellUmbraSet( const void * nif, std::vector<WwUmbraBlockIn> && blocks );
//! The depth probe (gate 2) wants a culling-off depth readback of this frame.
bool wwCellUmbraDepthWanted( Scene * scene );
//! While set, the camera pass draws every run whole and counts nothing (the depth probe's own draw).
void wwCellUmbraProbing( bool on );
//! The probe's depth (GL window depth, `w` x `h`, bottom row first) unprojected to world points into depth.txt.
void wwCellUmbraDepthWrite( Scene * scene, const float * depth, int w, int h );
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
