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

/* The cell view's far field: appends the worldspace's LOD (terrain rings, objects, tree cards)
 * under one "FarLOD" node of the cell document, so it draws with the cell's own programs.
 * Master: QSettings CellView/FarLod2 (ships ON since PERFFIX4; red WW_CELL_FARLOD_DEFRED=1 = the old OFF key);
 * WW_CELL_FARLOD=1/0 overrides. Read at cell open. */

bool cellFarLodWanted();
void cellFarLodSetWanted( bool on );
/* lane LODLIVE1: Far LOD off set live keeps the far data built and skips it at draw time (Node::isHidden on the
 * "FarLOD" subtree, the cards draw 0); on again shows the kept data, nothing rebuilt, nothing reopened. */
extern bool g_wwFarLodHidden;
inline bool cellFarLodHidden() { return g_wwFarLodHidden; }
void cellFarLodSetHidden( bool on );

/*! LOD type: Vanilla = only the game's .btr/.bto; FO4CS = our .lodl/.lodi/cards, the game's chunks filling gaps.
 *  QSettings CellView/FarLodType; WW_CELL_FARLOD_TYPE=vanilla|fo4cs pins it. */
enum WwCellFarLodType { WwFarLodAuto = 0, WwFarLodVanilla = 1, WwFarLodFo4cs = 2 };
int cellFarLodTypePinned();                  //!< the setting: auto, vanilla or fo4cs
void cellFarLodSetType( int type );
int cellFarLodLastType();                    //!< what the last build resolved to (the drop-down shows it on auto)
/*! lane LARGEREF3 (LARGEREF-FLAT): what the far objects this open will build can do with one ref's far copy, from the
 *  inputs cellFarLodAppend decides by (largeref.h LargeRef::Far): 0 none, 1 by ref (our instanced rings), 2 flat
 *  (flattened rings or the game's .bto, cell masks only). `lodlFound` = an FO4CSLOD .lodl exists for the world. */
int cellFarLodObjectsFar( bool lodlFound, QString * why = nullptr );
//! lane LIVESET4: a live set cut the document's far section: the draw forgets its far field
void cellFarLodForget( const void * nif );
namespace LargeRef { class FarMap; }
/*! lane LARGEREF4 (LARGEREF-FLAT-MIXED, LARGEREF-BUDGET): the far kind cell by cell as the document's far field was
 *  built (ours by ref, flat, none), for the stream's large refs. False: no far build on this document. */
bool cellFarLodLargeRefMap( const void * nif, LargeRef::FarMap & out );

/*! With the FO4CS type the near cells must load from the stack the far LOD was baked from (<ws>.lodb),
 *  or the block shows vanilla trees under a modded far field. Called BEFORE the worldspace loads;
 *  bake files that are gone are skipped and logged. `line` is the "far lod: stack ..." telemetry. */
struct CellFarStack
{
	bool adopt = false;
	QString plugins;
	QStringList resources;
	QString line;
	/*! On a non-adopted mismatch, the bake's resource roots still on disk; the far models' materials live
	 *  there (else they draw magenta/white). Appended after the session's stack. */
	QStringList bakeRoots;
	/*! The bake record read (empty: none) and its plugins still on disk, for the mod picker. */
	QString lodb;
	QStringList bakePresent;
};
CellFarStack cellFarLodStackFor( const QString & ws, const QString & lodlPath, const QString & nearPlugins );
/*! Append the roots the session's resource folders lack, LAST: the archive index keeps the FIRST copy
 *  of a path, so only paths nothing else answers come from the bake's roots. Returns the log line. */
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
};

QString cellFarLodAppend( NifModel * nif, const QModelIndex & iRoot, const EsmWorld & world,
	const QString & ws, int cx, int cy, int n, const QString & lodlPath, const Vector3 & origin,
	CellFarStream * stream = nullptr );

//! How far (scene units, from the scene origin) the far field of the scene's document reaches; 0 = none.
float wwCellFarLodReach( const Scene * scene );
//! the far field's center in scene units (the reach is measured from it)
Vector3 wwCellFarLodOrigin( const Scene * scene );

//! Before the scene draws: the far sway uniforms (zeroed once when no far field is active).
void wwCellFarLodFrame( Scene * scene );
/*! lane SHADOWSHIMMER2 (FARSWAYSHADOW): the far sway of THIS paint, one sample a paint (the clock read once), so the
 *  colour pass's uniforms (wwCellFarLodFrame) and the sun cascades' casters (gl/sunshadow.cpp) sway by the same values.
 *  amp 0 = no sway (no far field, or WW_CELL_FARLOD_SWAY=0); block: the loaded block's centre in view space and its half
 *  edge scaled (the uniform farSwayBlock), world: its centre x, y and half edge in scene units (unscaled). */
struct WwFarSway
{
	float amp = 0.0f, time = 0.0f;
	float block[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float world[3] = { 0.0f, 0.0f, 0.0f };
};
WwFarSway wwCellFarLodSway( Scene * scene );
//! lane GAPFIX1: the last frame drew a moving far sway, so the view paints again (never in a harness)
bool wwCellFarLodWantsRepaint();

/*! The far tree cards, drawn with the cell's lights, fog and imagespace. Called twice a frame (inside the
 *  HDR frame before its resolve, and after); they draw in the first call when the frame is HDR. Returns cards drawn. */
int wwCellFarLodCards( Scene * scene, bool insideHdr, bool hdrFrame );

/*! MODULES1 part 27 (bungo 11-08, "rendering order"): the lit cards drawn between the cell's opaque and sorted passes,
 *  so smoke and every blended effect land over them and the soft fade reads their depth (they drew after the
 *  particles: a far card stood crisp in front of the smoke around it). paintGL arms it before scene->draw();
 *  Scene::drawShapes draws it once; wwCellFarLodDrewEarly() disarms and says whether the late call must still draw.
 *  Red WW_CELL_FARLOD_LATE=1: the old place, after the scene. */
void wwCellFarLodArmEarly( bool hdrFrame );
void wwCellFarLodEarly( Scene * scene );
bool wwCellFarLodDrewEarly();

//! The near plane while the far field is on: `nr` raised to 16 (the game's fNearDistance 15, rounded up);
//! WW_CELL_FARLOD_NEAR forces one.
double wwCellFarLodNear( double nr );

//! lane PERFLOAD1: the near plane never passes the far land. `nr` is the loaded block's own near plane (through
//! wwCellFarLodNear); it comes back capped to the camera's height over the far field's top times the cosine of the
//! frustum's corner angle, 16 at the least. Red WW_CELL_FARLOD_NEAR_RED=bounds (the loaded block's plane alone).
double wwCellFarLodNearCap( const Scene * scene, double nr, double tanHalfFovY, double aspect );

//! The planes the projection took with the far field on, logged on change with their depth step.
void wwCellFarLodPlanes( double nearPlane, double farPlane );

//! WW_CELL_FARLOD_SKYCENSUS=1: counts the finished frame's below-horizon pixels no surface reached.
void wwCellFarLodSkyCensus( Scene * scene );

#endif // CELLFARLOD_H
