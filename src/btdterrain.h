/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef BTDTERRAIN_H
#define BTDTERRAIN_H

#include <QModelIndex>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <utility>
#include <vector>

class NifModel;
class QWidget;

//! What a .btd file's header says about its worldspace, read without
//! decompressing anything. Cell coordinates are game cells (4096 units).
struct BtdWorldInfo
{
	int cellMinX = 0, cellMinY = 0, cellMaxX = 0, cellMaxY = 0;
	float heightMin = 0.0f, heightMax = 0.0f;
	int landTextureCount = 0;
	int groundCoverCount = 0;
};

//! One region choice: an inclusive cell rectangle and a detail level.
//! lod 0 samples every 32 units (the file's full resolution); each level
//! halves that, so lod 4 is one sample every 512 units.
struct BtdRegionSpec
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	int lod = 4;
	bool valid = false;
};

//! Read the .btd header only. False (with *error set) on a file that is not
//! version 5/6 BTD.
bool btdReadWorldInfo( const QString & path, BtdWorldInfo & info, QString * error );

//! How many BSTriShapes and vertices a region would generate, for dialogs and
//! guards. Returns false if the region is outside the file or over budget,
//! with *error naming the limit.
bool btdEstimateRegion( const BtdWorldInfo & info, const BtdRegionSpec & spec,
	qint64 * shapes, qint64 * verts, QString * error );

/*! Build a Fallout 4 document whose geometry is the region's terrain.
 *
 * One BSTriShape per tile of cells (as many cells as keep a tile under the
 * 65,535-vertex format limit), heights from the file's heightmaps, normals
 * from finite differences on the same grid, seams closed by sampling one row
 * into the neighbouring cell. The model is rebuilt from scratch (createNew).
 */
bool nifCreateBtdTerrainScene( NifModel * nif, const QString & btdPath,
	const BtdRegionSpec & spec, QString * error );

//! The WW_BTD_REGION="x0,y0,x1,y1,lod" environment override, shared by the
//! dialog and the no-dialog open paths. False when the variable is unset or
//! malformed.
bool btdRegionFromEnv( BtdRegionSpec & spec );

//! Modal region/detail picker. Returns false if cancelled. When the
//! environment variable WW_BTD_REGION is set to "x0,y0,x1,y1,lod" the dialog
//! is skipped and that region is returned, which is how harnesses drive this.
bool btdQueryRegion( QWidget * parent, const QString & path,
	const BtdWorldInfo & info, BtdRegionSpec & spec );

//! The region a bare open uses when no dialog ran (command line, harness):
//! the whole worldspace at lod 4.
BtdRegionSpec btdDefaultRegion( const BtdWorldInfo & info );


/* -------------------------------------------------------------------------
 * .lodl — our own whole-worldspace landscape file (docs/LODGEN_BTD_FORMAT.md)
 *
 * It was written and opened as `.lodt` until 2026-09-09; `.lodt` names the
 * terrain TEXTURE sheets now (src/io/lodvfile.h). The C++ names below still
 * say `lodt` and are internal: read every one of them as "the .lodl file".
 *
 * Same species of problem as a .btd and therefore the same route: the file
 * stores no triangles, so opening one means choosing a region and a detail
 * level and MESHING it here. Both live in btdterrain.cpp because they share
 * the tile mesher — the shape that closes seams, respects the 65,535-vertex
 * ceiling and writes a descriptor the renderer accepts is one piece of code,
 * not two (CONSTITUTION rule 10, "what is shared lives in the shared code").
 *
 * The one thing a .lodl has that a .btd does not is PLANES. Heights are the
 * geometry; every other stored plane paints the same surface as vertex
 * colours, one at a time, so each is viewable on its own.
 * ------------------------------------------------------------------------- */

//! Which stored plane paints the surface. Heights are the geometry, so the
//! Height view carries no vertex colours at all and is the same scene a .btd
//! of the same terrain builds.
enum class LodtPlane
{
	Height = 0,
	AmbientOcclusion,
	LandTextureBlend,
	TerrainColour,
	GroundCover,
	WaterHeight,
	WaterType,
	CellFlags,
	CellHeightRange,
	CoarseOverview,
	/* Version 3's water planes. They are per-TEXEL, not per-cell like
	 * WaterHeight and WaterType above -- which is the whole point of them:
	 * sixteen WATR forms serve hundreds of separate bodies, and only an ID
	 * per texel can give two lakes two colours. */
	WaterBodyId,
	WaterFlow,
	WaterShore,
	/* Not a stored plane: the body's water height minus the file's own
	 * full-rate ground under the texel, worked out at draw time from two
	 * things version 3 already holds. */
	WaterDepth,
	Count
};

//! Stable machine name ("height", "ao", ...) for the environment override,
//! the CLI and the harness; never translated.
const char * lodtPlaneKey( LodtPlane plane );
//! Human label for the picker.
QString lodtPlaneLabel( LodtPlane plane );
//! Parse a machine name. Returns false on an unknown one.
bool lodtPlaneFromKey( const QString & key, LodtPlane & plane );

//! What a .lodl's header and tables say, read without inflating any block.
struct LodtWorldInfo
{
	int cellMinX = 0, cellMinY = 0, cellMaxX = 0, cellMaxY = 0;
	float heightMin = 0.0f, heightMax = 0.0f, heightQuantum = 8.0f;
	int samplesPerCell = 32, blockEdge = 32, levelCount = 4;
	int aoSamples = 0, overviewSamples = 0;
	int ltexCount = 0, watrCount = 0, gcvrCount = 0, blockCount = 0;
	quint32 sectionFlags = 0;
	//! version 3: 0 when the file carries no such plane (check the FLAG, then this).
	int bodyCount = 0, bodySamples = 0, flowSamples = 0, shoreSamples = 0;
	int headerVersion = 1;
};

/*! One .lodl view: an inclusive cell rectangle, a detail level and a plane.
 *
 * lod 0 is the file's own sample rate (`samplesPerCell` a cell); each level
 * halves it, so the spacing in world units is 4096 / (samplesPerCell >> lod).
 * That is the SAME meaning lod carries on the .btd route — the number of
 * halvings — but the units differ because the two formats sample at
 * different rates, which is exactly why it is a header field.
 */
struct LodtRegionSpec
{
	int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	int lod = 2;
	LodtPlane plane = LodtPlane::Height;
	bool valid = false;
};

//! Read the .lodl header and tables only. False (with *error set) when the
//! file is not a .lodl or does not describe itself consistently. A terrain
//! TEXTURE file (.lodt) handed to this route is refused BY NAME, not by
//! "bad magic" -- src/lodtfile.cpp, LodtFile::open.
bool lodtReadWorldInfo( const QString & path, LodtWorldInfo & info, QString * error );

//! Which planes this particular file actually carries, in enum order. A file
//! without a section must refuse that plane in words rather than draw black.
QList<LodtPlane> lodtAvailablePlanes( const LodtWorldInfo & info );

//! Coarsest usable detail level: one sample a cell.
int lodtMaxLod( const LodtWorldInfo & info );

//! Shapes and vertices a region would generate; false with *error naming the
//! limit when it leaves the worldspace or is over budget.
bool lodtEstimateRegion( const LodtWorldInfo & info, const LodtRegionSpec & spec,
	qint64 * shapes, qint64 * verts, QString * error );

/*! Build a Fallout 4 document whose geometry is the region's terrain, painted
 *  with the spec's plane. Same tile mesher, seam rule and vertex descriptor as
 *  the .btd route; the only difference is the vertex-colour channel, which is
 *  absent on the Height plane so that view is bit-for-bit the .btd scene.
 *  `notes`, when given, receives what the plane measured (its value range, how
 *  many samples carried anything) so a picture is never the only evidence. */
bool nifCreateLodtTerrainScene( NifModel * nif, const QString & lodtPath,
	const LodtRegionSpec & spec, QString * error, QString * notes = nullptr );

//! WW_LODL_REGION="x0,y0,x1,y1,lod[,plane]" — the no-dialog override, the same
//! shape WW_BTD_REGION has. WW_LODL_PLANE sets the plane on its own. The old
//! WW_LODT_* spellings are REFUSED loudly, never silently honoured.
bool lodtRegionFromEnv( LodtRegionSpec & spec );

//! Modal region/detail/plane picker; false if cancelled. Skipped when
//! WW_LODL_REGION is set, which is how harnesses and renders drive this.
bool lodtQueryRegion( QWidget * parent, const QString & path,
	const LodtWorldInfo & info, LodtRegionSpec & spec );

//! The view a bare open uses when no dialog ran: the whole worldspace, heights,
//! at the coarsest level that fits the budget from 8 samples a cell down.
LodtRegionSpec lodtDefaultRegion( const LodtWorldInfo & info );

/* -------------------------------------------------------------------------
 * lane FARLOD1 -- the cell view's far terrain: rings of `.lodl` ground around
 * the loaded block, APPENDED under a node of the cell document (the same
 * meshing as the `.lodl` view, buildTerrainSurface, not a second one).
 *
 * Ring i's cut is ring i-1's outer rectangle as built (ring 0's is the
 * caller's: the loaded block), so no cell is drawn twice. Each ring snaps its
 * outer rectangle OUTWARD to whole tiles of its own sheet level and is clipped
 * to what the sheets and the `.lodl` hold. A ring's outer edge is stitched to
 * the next ring's lattice (off-lattice heights lerped), and ring 0's inner edge
 * can be snapped to the loaded LAND through `innerZ`.
 * ------------------------------------------------------------------------- */
struct LodlFarRing
{
	int lod = 2;                //!< `.lodl` level: samples a cell = file rate >> lod
	int sheetDim = 0;           //!< VT level (cells a sheet tile); 0 = the finest on disk
	int x0 = 0, y0 = 0, x1 = -1, y1 = -1;     //!< asked outer rectangle, inclusive cells
	int cx0 = 0, cy0 = 0, cx1 = -1, cy1 = -1; //!< ring 0 only: the cut (the loaded block)
	bool noCut = false;         //!< red control: draw the cut cells too
	bool snapInner = false;     //!< ring 0: inner-edge heights from innerZ
	//! world x, y -> the loaded LAND's height there; false = no LAND
	std::function<bool( float, float, float * )> innerZ;
	QString prefix = QStringLiteral( "FarLOD terrain" );
	/*! Rings built one call each (the vanilla fill needs each ring's footprint before the next): the cut
	 *  is cx* even after an earlier ring of the call, and the next ring's level (its seam lerp) is given. */
	bool cutGiven = false;
	int nextLod = 0;            //!< 0 = no next ring
	//! FINALFIX: write only the asked rectangle (and the cut), not the whole sheet tiles around it.
	bool exact = false;
	//! Our coverage limit (a mixed run, WW_CELL_FARLOD_OURS): whole sheet tiles inside it; none when lx1 < lx0.
	int lx0 = 0, ly0 = 0, lx1 = -1, ly1 = -1;

	// ---- out, read back from what was built
	bool built = false;
	int ox0 = 0, oy0 = 0, ox1 = -1, oy1 = -1;
	int n = 0;
	qint64 estVerts = 0, shapes = 0, verts = 0, tris = 0, cutTris = 0;
	int sheetTiles = 0;
	QString sheetFile;
	qint64 seamSamples = 0;
	double seamBeforeMax = 0, seamBeforeMean = 0, seamAfterMax = 0, seamAfterMean = 0;
	QStringList notes;
};

//! Appends every ring under `parent` (the caller holds updates and runs updateModel).
//! `shift` is subtracted from each shape's Translation. A ring whose estimated
//! vertices take the running total past `maxVerts` (> 0) is not built, and says so.
bool nifAppendLodlFarRings( NifModel * nif, const QModelIndex & parent, const QString & lodlPath,
	const float shift[3], std::vector<LodlFarRing> & rings, qint64 maxVerts, QString * error );

/*! Lane FARLOD1: the far field's WATER, one ring of it. The `.lodl`'s water bodies (lane WATER1's
 *  body plane, its surface plane and each body's WATR form) as flat sheets at the body's height,
 *  `texels` body-plane samples a cell edge (coarser farther out), the cut left dry. Grown one texel
 *  onto dry ground below a neighbour's surface, so the terrain's depth draws the shore (the game's
 *  plane-cut-by-ground). Built per WATR form so each shape takes its own record. */
struct LodlFarWater
{
	int x0 = 0, y0 = 0, x1 = -1, y1 = -1;     //!< the ring, inclusive cells
	int cx0 = 0, cy0 = 0, cx1 = -1, cy1 = -1; //!< the cut, inclusive cells (none when cx1 < cx0)
	bool noCut = false;         //!< red control: the cut cells get water too
	int texels = 4;             //!< body-plane samples a cell edge, wanted (clamped to the file's)
	QString prefix = QStringLiteral( "FarLOD water" );

	// ---- out
	int rate = 0;               //!< the samples a cell edge it took
	qint64 quads = 0, wetTexels = 0, grownTexels = 0;
	int shapes = 0;
	QSet<quint16> bodies;
	std::vector<std::pair<int, quint32>> blocks;   //!< each shape's block and WATR form
	QStringList notes;
};

//! Appends every water ring under `parent` (the caller holds updates and runs updateModel).
//! False (with `error`) when the file has no body plane; nothing is appended then.
bool nifAppendLodlFarWater( NifModel * nif, const QModelIndex & parent, const QString & lodlPath,
	const float shift[3], std::vector<LodlFarWater> & rings, QString * error );

#endif // BTDTERRAIN_H
