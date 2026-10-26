/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLFARLOD_H
#define CELLFARLOD_H

#include <QModelIndex>
#include <QString>
#include <QStringList>

#include <array>
#include <vector>

class EsmWorld;
class NifModel;
class Scene;
class Vector3;

/* ---------------------------------------------------------------------------
 * lane FARLOD1 -- the cell view's far field.
 *
 * The cell view loads the game's block (uGridsToLoad, 5x5) at full detail; past
 * it the game draws the worldspace's LOD: terrain, objects and tree cards out to
 * fBlockMaximumDistance. This appends that LOD to the cell document itself, under
 * one NiNode "FarLOD" of the cell root, through the paths that already read it:
 *
 *   terrain  nifAppendLodlFarRings (btdterrain) -- the .lodl view's meshing and
 *            its VT sheets, in rings coarser with distance (the clipmap rings),
 *            the loaded block cut out, ring 0's inner edge on the loaded LAND;
 *   objects  nifAppendLodiObjects (lodinative) -- the .lodi/.lodo view's soup,
 *            the authored slot per ring, the block (and the ring inside) cut out;
 *   cards    ImpostorChunk::armCellFar -- the chunk manifests' tree cards.
 *
 * Being document shapes, terrain and objects draw with the cell's own programs
 * (cell lights, fog, weather) and need no renderer of their own.
 *
 * Master: the cell workspace's "Far LOD" row (QSettings CellView/FarLod, ships
 * OFF); WW_CELL_FARLOD=1/0 wins for a measuring run. Read at cell open.
 * Every census line starts "far lod:" (stdout, the .notes a shot captures).
 * ------------------------------------------------------------------------- */

bool cellFarLodWanted();
void cellFarLodSetWanted( bool on );

/*! The LOD type (the row's drop-down): Vanilla = only the game's .btr/.bto; FO4CS = our .lodl/.lodi/cards,
 *  the game's chunks filling any we do not have (cellfarvanilla.h). QSettings CellView/FarLodType
 *  ("vanilla" | "fo4cs"; absent = FO4CS when our files exist for the worldspace, else Vanilla);
 *  WW_CELL_FARLOD_TYPE=vanilla|fo4cs pins it for a measuring run. */
enum WwCellFarLodType { WwFarLodAuto = 0, WwFarLodVanilla = 1, WwFarLodFo4cs = 2 };
int cellFarLodTypePinned();                  //!< the setting: auto, vanilla or fo4cs
void cellFarLodSetType( int type );
int cellFarLodLastType();                    //!< what the last build resolved to (the drop-down shows it on auto)
//! lane LIVESET4: a live set cut the document's far section: the draw forgets its far field
void cellFarLodForget( const void * nif );

/*! lane FARLOD2: ONE STACK FOR NEAR AND FAR. The FO4CS far LOD was baked from a load order and a resource
 *  stack, and the bake recorded both (`<ws>.lodb` beside the `.lodl`: one `plugin` line a plugin, one
 *  `resource` line a root -- lane BAKEREC1). With the FO4CS type the near cells must load from the same stack,
 *  or the block shows vanilla trees under a far field of the mod's. Called BEFORE the worldspace loads:
 *  `adopt` = the near cells take `plugins` (comma list, the bake's order) and `resources` (the bake's stack,
 *  last wins). `line` is the telemetry ("far lod: stack ..."), always set when the far field is wanted:
 *  a match, an adoption with what moved, or a refusal with its reason. The Vanilla type never adopts.
 *  WW_CELL_FARLOD_RED=ownstack keeps the caller's load order and says MISMATCH (the gate's red control).
 *  lane FARLOD3: a bake file that is gone no longer refuses the adoption -- it is skipped and logged ("far lod:
 *  stack skips <path>"), the rest of the bake's stack is adopted. WW_CELL_FARLOD_RED=refusemissing: the old rule. */
struct CellFarStack
{
	bool adopt = false;
	QString plugins;
	QStringList resources;
	QString line;
	/*! lane FARLOD3: on a MISMATCH that is not adopted (only the refusemissing red, or no bake plugin left), the bake's recorded resource roots that are still on
	 *  disk. The far field's OWN models name materials and textures that live there (his BNS tree LOD:
	 *  materials/bns/lod/... in "BNS Trees - Main.ba2"); without them the far trees drew magenta (legacy) or
	 *  white (PBR). `cellFarLodLendRoots` appends them AFTER the session's stack. Empty on match/adopt/red. */
	QStringList bakeRoots;
	/*! lane MODPICK1: the bake record read (empty: none -- not FO4CS, or no readable .lodb) and its plugins still on
	 *  disk in the bake's order, so the mod picker can list the bake and say when the near cells differ from it. */
	QString lodb;
	QStringList bakePresent;
};
CellFarStack cellFarLodStackFor( const QString & ws, const QString & lodlPath, const QString & nearPlugins );
/*! lane FARLOD3: append the roots the session's FO4 resource folders lack, LAST -- the archive index keeps
 *  the FIRST copy of a path (libfo76utils ba2file.cpp), so every file the near cells' stack already has still
 *  comes from it; only paths nothing else answers (the far field's own) are served by the bake's roots.
 *  Returns the "far lod: resources ..." line. WW_CELL_FARLOD_RED=nobakeroots lends nothing (and, lane
 *  FIXPACK1, cellFarLodStackFor then adopts the bake's plugins without its resource roots). */
QString cellFarLodLendRoots( const QStringList & bakeRoots );
//! Make `resources` the session's resource stack (the generator's and the texture folders), as main.cpp
//! does for WW_LODGEN_RESOURCES. Nothing when it already is.
void cellFarLodAdoptResources( const QStringList & resources );

/*! Appends the far field under `iRoot` (the caller holds updates and runs
 *  updateModel). `origin` is the cell scene's own origin (it is origin-relative).
 *  Returns the census, one "  far lod: ..." line each, for the cell's notes. */
// lane CELLSTREAM1: a streaming open's far field -- ring 0 uncut over the roam area, one shape a cell
struct CellFarStream
{
	int roam = 4;                                   //!< in: cells the camera may roam from the open's centre
	std::vector<std::array<int, 3>> handBlocks;     //!< out: each ring-0 terrain/water shape's block, cell x, y
	int landBlocks = 0;                             //!< out: how many of them (the first) are terrain
	int objX0 = 0, objY0 = 0, objX1 = -1, objY1 = -1;   //!< out: the far objects' and cards' hole (cells)
	//! lane FARHOLE: out: the far objects' triangles by placement cell { block, x, y, first, count }; a streamed
	//! cell hides its own when it loads (past the hole the camera can roam on, the far field had it drawn twice)
	std::vector<std::array<int, 5>> objTris;
};

QString cellFarLodAppend( NifModel * nif, const QModelIndex & iRoot, const EsmWorld & world,
	const QString & ws, int cx, int cy, int n, const QString & lodlPath, const Vector3 & origin,
	CellFarStream * stream = nullptr );

//! How far (scene units, from the scene origin) the far field of the scene's document reaches; 0 = none.
float wwCellFarLodReach( const Scene * scene );
//! the far field's centre in scene units (the reach is measured from it)
Vector3 wwCellFarLodOrigin( const Scene * scene );

//! Before the scene draws: the far sway uniforms (zeroed once when no far field is active).
void wwCellFarLodFrame( Scene * scene );
//! lane GAPFIX1: the last frame drew a moving far sway, so the view paints again (never in a harness)
bool wwCellFarLodWantsRepaint();

/*! The far tree cards, drawn with the cell's lights, fog and imagespace (impostor_cell.prog). Called
 *  twice a frame: inside the HDR frame before its resolve (`insideHdr`), and after it; they draw in the
 *  first call when the frame is HDR (`hdrFrame`), else in the second. WW_CELL_FARLOD_RED=cardsafter is
 *  the old path, the red control: after the resolve, unlit by the cell. Returns the cards drawn. */
int wwCellFarLodCards( Scene * scene, bool insideHdr, bool hdrFrame );

//! The near plane while the far field is on: the view's own near `nr`, raised to 16 (the game's
//! fNearDistance 15, rounded up); WW_CELL_FARLOD_NEAR forces one for a measuring run (the near gate's reds).
double wwCellFarLodNear( double nr );

//! The planes the projection took with the far field on, logged on change with their depth step.
void wwCellFarLodPlanes( double nearPlane, double farPlane );

//! WW_CELL_FARLOD_SKYCENSUS=1: counts the finished frame's below-horizon pixels no surface reached.
void wwCellFarLodSkyCensus( Scene * scene );

#endif // CELLFARLOD_H
