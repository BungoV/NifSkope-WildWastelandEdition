/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

#ifndef CELLFARLOD_H
#define CELLFARLOD_H

#include <QModelIndex>
#include <QString>

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

/*! Appends the far field under `iRoot` (the caller holds updates and runs
 *  updateModel). `origin` is the cell scene's own origin (it is origin-relative).
 *  Returns the census, one "  far lod: ..." line each, for the cell's notes. */
QString cellFarLodAppend( NifModel * nif, const QModelIndex & iRoot, const EsmWorld & world,
	const QString & ws, int cx, int cy, int n, const QString & lodlPath, const Vector3 & origin );

//! How far (scene units, from the scene origin) the far field of the scene's document reaches; 0 = none.
float wwCellFarLodReach( const Scene * scene );

//! Before the scene draws: the far sway uniforms (zeroed once when no far field is active).
void wwCellFarLodFrame( Scene * scene );

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
