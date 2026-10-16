#ifndef CELLCULL_H
#define CELLCULL_H

#include "data/niftypes.h"

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

class Scene;
class Shape;

/*! Game-style culling for the cell view: welded shapes keep per-placement triangle RUNS with bounding spheres, and
 *  the camera pass draws only runs in the frustum, then applies previs (occluders, rooms / portals, the Umbra tome).
 *  Shadow casters are never culled by the camera. WW_CELL_CULL=0|1 and WW_CELL_PREVIS=0|1 pin the rows (both ship
 *  OFF); WW_CELL_CULL_RED=casters and WW_CELL_PREVIS_RED=casters wrongly cull casters (debug control). */
struct WwCullRun
{
	std::uint32_t first = 0, count = 0;	//!< triangles [first, first + count) of the shape
	Vector3 center;				//!< bounding sphere, the shape's own space
	float radius = 0.0f;
	std::uint32_t ref = 0;			//!< the placed reference's form id (0 = none)
};

/*! One previs primitive in the cell document's space: center, unit axes, half extents. `type` 1 box, 3 plane
 *  (thinnest axis is the normal); a portal names its rooms (`from`, `to`, 0 = outside). */
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

/*! One loaded previs block for the tome cull (src/gl/cellumbra.h): its .uvd bytes and its cells' XCRI refs with
 *  the 0xFD cell code (x mod 32) << 19 | (y mod 32) << 14 (0 for an interior). Runs the tome does not govern keep
 *  the frustum / occluder path. WW_CELL_UMBRA=0 disables it; WW_CELL_UMBRA_RED, _DUMP=<dir>, _DEPTH=1: debug aids. */
struct WwUmbraBlockIn
{
	std::uint32_t block = 0;	//!< the RVIS form
	QByteArray tome;		//!< the .uvd file
	std::vector<std::pair<std::uint32_t, std::uint32_t>> combined;	//!< (XCRI ref form id, 0xFD cell code)
};
void wwCellUmbraSet( const void * nif, std::vector<WwUmbraBlockIn> && blocks );
//! The depth probe wants a culling-off depth readback of this frame.
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
