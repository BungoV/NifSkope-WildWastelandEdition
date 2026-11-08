#ifndef CELLCULL_H
#define CELLCULL_H

#include "cellblockmap.h"
#include "data/niftypes.h"

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <vector>

class Scene;
class Shape;

/*! Game-style culling for the cell view: welded shapes keep per-placement triangle RUNS with bounding spheres, and
 *  the camera pass draws only runs in the frustum, then applies previs (occluders, rooms / portals, the Umbra tome).
 *  Shadow casters are never culled by the camera. WW_CELL_CULL=0|1 and WW_CELL_PREVIS=0|1 pin the rows. Culling ships
 *  ON (PERFHUNT2 show 11-03: Concord -1.7 ms GPU, Memory Den -2.4 ms CPU, stills within noise; WW_CELL_CULL_RED=off is
 *  the old default); previs ships ON from MODULES1 part 19 (it was OFF after the same show: Concord +5 ms with 63k runs
 *  occluded, Memory Den within noise; WW_CELL_PREVIS_RED=off is that old default).
 *  WW_CELL_CULL_RED=casters and WW_CELL_PREVIS_RED=casters wrongly cull casters (debug control). */
struct WwCullRun
{
	std::uint32_t first = 0, count = 0;	//!< triangles [first, first + count) of the shape
	Vector3 center;				//!< bounding sphere, the shape's own space
	float radius = 0.0f;
	std::uint32_t ref = 0;			//!< the placed reference's form id (0 = none)
	//! lane OBJFADE1: the placement's whole bound (all its shapes) in the same space, and its fade class (0 = none)
	Vector3 fadeCenter;
	float fadeRadius = 0.0f;
	std::uint8_t fadeType = 0;
};

namespace WwObjFade { struct Config; }

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
	bool interior = false;	//!< lane PERFFIX4: an interior takes previs whatever the row (src/features/near/cellprevisrule.h)
};
//! The document's previs scene (replaced whole; wwCellCullBegin forgets it).
void wwCellPrevisSet( const void * nif, WwPrevisScene && sc );

/*! One loaded previs block for the tome cull (src/features/near/cellumbra.h): its .uvd bytes and its cells' XCRI refs with
 *  the 0xFD cell code (x mod 32) << 19 | (y mod 32) << 14 (0 for an interior). Runs the tome does not govern keep
 *  the frustum / occluder path. WW_CELL_UMBRA=0 disables it; WW_CELL_UMBRA_RED, _DUMP=<dir>, _DEPTH=1: debug aids. */
struct WwUmbraBlockIn
{
	std::uint32_t block = 0;	//!< the RVIS form
	QByteArray tome;		//!< the .uvd file
	bool stale = false;		//!< lane PERFFIX4: a later plugin touched its cells' refs (refused unless WW_CELL_UMBRA_RED=stale)
	std::vector<std::pair<std::uint32_t, std::uint32_t>> combined;	//!< (XCRI ref form id, 0xFD cell code)
};
void wwCellUmbraSet( const void * nif, std::vector<WwUmbraBlockIn> && blocks );
//! The depth probe wants a culling-off depth readback of this frame.
bool wwCellUmbraDepthWanted( Scene * scene );
//! While set, the camera pass draws every run whole and counts nothing (the depth probe's own draw).
void wwCellUmbraProbing( bool on );
//! The probe's depth (GL window depth, `w` x `h`, bottom row first) unprojected to world points into depth.txt.
void wwCellUmbraDepthWrite( Scene * scene, const float * depth, int w, int h );
//! MODULES1: the probe pass itself (features/near/cellumbradepth.cpp), called by GLView::paintGL
void wwCellUmbraDepthPass( Scene * scene );
bool wwCellPrevisOn();
void wwCellPrevisSetOn( bool on );
//! lane PERFFIX4: whether previs applies to a document of this kind now (an interior always; src/features/near/cellprevisrule.h)
bool wwCellPrevisFor( bool interior );
//! 1 = the red control `casters`
int wwCellPrevisRed();

//! A new scene for this document: its table is emptied.
void wwCellCullBegin( const void * nif );
//! The written shape `block` draws in these runs (in triangle order, covering every triangle).
void wwCellCullShape( const void * nif, int block, std::vector<WwCullRun> && runs );
// lane CELLSTREAM1: a streamed cell let go of this block (it is about to be reused)
void wwCellCullForget( const void * nif, int block );
//! lane LIVESET3: the document's blocks were renumbered (a live set removed or moved some)
void wwCellCullRemap( const void * nif, const WwBlockMap & m );
/*! Lane DECALS1 (2026-10-23): THE GAME'S DECAL LOD FADE. A model shape with the Decal / Dynamic_Decal lighting
 *  flags fades per placement: x = distance x (FOV / 80) / (mult x 7200); alpha 1 up to x = 0.05, linear to 0 at
 *  0.06, not drawn past it. mult = fLODFadeOutMultObjects (30 in bungo's Prefs; pin WW_CELL_DECAL_FADE_MULT).
 *  Placed TXST decals never fade (they go with their receiver). Row "Decal distance fade" (WwLook::DecalFade).
 *  Red WW_CELL_DECAL_FADE_RED=near: mult 1 (fades at ~410-490 units, FOV 70). */
void wwCellDecalFadeShape( const void * nif, int block, const std::vector<WwCullRun> & runs );
/*! The fade's ranges: `keep` drawn whole, `band` (first, count) pairs drawn with `bandAlpha` each. False = not a
 *  fading decal shape (draw as before). The Culling row's frustum applies too; previs does not. */
bool wwCellDecalFadeRanges( Scene * scene, const Shape * sh, int block, std::int64_t numTris,
	std::vector<std::uint32_t> & keep, std::vector<std::uint32_t> & band, std::vector<float> & bandAlpha );
/*! Lane DECALS1: a placed decal shape whose texture set has a height map (TX04): the parallax scale and passes
 *  of its DODT. Row "Decal parallax" (WwLook::DecalParallax). */
void wwCellDecalPomShape( const void * nif, int block, float scale, int passes );
//! (scale x 0.1, the step cap clamp(passes, 16, 72)) for the shape, or false.
bool wwCellDecalPomFor( const void * nif, int block, float & scale, float & steps );
/*! Lane OBJFADE1 (GFXR-GEO G1): THE GAME'S PER-CLASS DISTANCE FADE of placed refs (src/features/near/cellobjfade.h), in
 *  wwCellCullCamera through the frame's camera: a run is hidden where the game switches its ref out. Row "Distance
 *  fade" under Objects (WwLook::ObjectFade, pin WW_CELL_OBJFADE), ships off. Red WW_CELL_OBJFADE_RED=decalonly. */
bool wwCellObjFadeOn();
const WwObjFade::Config & wwCellObjFadeConfig();
//! LOCMULT: the document's location "Actor Fade Mult" (LCTN ANAM; 0 = none), the actors' multiplier x it
void wwCellObjFadeSetLocation( const void * nif, float actorFadeMult );
//! lane BINDLESSMDI1: wwCellDecalFadeRanges may draw the shape its own way (no side effect)
bool wwCellDecalFadeHas( Scene * scene, int block );
/*! Lane DECALANGLE: a placed decal shape whose vertex alpha is dot(N, D) (src/celldecalangle.h): the shader runs
 *  the game's angle rule per pixel. Off for the red WW_CELL_DECAL_RED=vertexangle. */
void wwCellDecalAngleShape( const void * nif, int block, bool mapped );
bool wwCellDecalAngleFor( const void * nif, int block );
//! Lane DECALANGLE2: the shape's vertex rgb is D and the rule reads the drawn normal of the shared opaque pass.
bool wwCellDecalMappedFor( const void * nif, int block );
//! Lane DECALANGLE2: any such shape in the document (the shared opaque pass is then kept for it).
bool wwCellDecalMappedHas( const void * nif );

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
	std::vector<std::uint32_t> & out, bool cachedLayer = false );
/*! lane PERFFIX6 PHASE 2 (MT): the caster cull on worker threads. True when this frame's caster cull takes the default
 *  branch (on, no red control, no Previs / tome red), which reads only data the main thread wrote; the call also settles
 *  the switch caches it reads, so call it on the main thread right before the fan-out. A worker binds its own tally
 *  (null = the shared counts, the main thread); the main thread adds every tally after the join, in any order (sums). */
struct WwCellCullTally
{
	std::int64_t casterFaded = 0, shadowOcc[3] = { 0, 0, 0 }, cDrawn[3] = { 0, 0, 0 }, cCulled[3] = { 0, 0, 0 };
	bool any = false;
};
bool wwCellCullCasterParallelOk();
void wwCellCullTallyBind( WwCellCullTally * tally );
void wwCellCullTallyMerge( const WwCellCullTally & tally );
/*! lane PCMB1 part B: cascade `cascade`'s receiver slab this frame -- the main camera's planar view depth (view units)
 *  that reads it, padded by the filter's reach; nearDepth very negative = no near side (cascade 0). ok false = no slab
 *  (the receiver cull off for it). A caster whose shadow, swept along the sun, never enters the slab is dropped, except
 *  on a cached static layer (`cachedLayer`) and under the red WW_CELL_CULL_RED=noshadowocc. */
void wwCellCullCasterSlab( int cascade, bool ok, float nearDepth, float farDepth );

//! Once per frame, before the passes: prints the last frame's counts when they changed.
void wwCellCullFrame();
/*! lane PERFFIX4: the frame's camera view (glview, right after wwCellCullFrame). Previs (occluders, rooms, the tome)
 *  culls only in passes that look through it (the obscurance, reflections and opaque walks); a pass with another eye
 *  (a lamp's cube face, a probe capture) draws every run, so no caster and no captured object is lost. */
void wwCellCullCameraView( const Transform & view );
//! The last frame's counts, one line.
QString wwCellCullSummary();

#endif
