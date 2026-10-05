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

/*! Appends the far field under `iRoot` (the caller holds updates and runs
 *  updateModel). `origin` is the cell scene's own origin (it is origin-relative).
 *  Returns the census, one "  far lod: ..." line each, for the cell's notes. */
QString cellFarLodAppend( NifModel * nif, const QModelIndex & iRoot, const EsmWorld & world,
	const QString & ws, int cx, int cy, int n, const QString & lodlPath, const Vector3 & origin );

//! How far (scene units, from the scene origin) the far field of the scene's document reaches; 0 = none.
float wwCellFarLodReach( const Scene * scene );

//! Before the scene draws: the far sway uniforms (zeroed once when no far field is active).
void wwCellFarLodFrame( Scene * scene );

//! After the scene (and its HDR resolve): the far tree cards. Returns the cards drawn.
int wwCellFarLodCards( Scene * scene );

//! WW_CELL_FARLOD_SKYCENSUS=1: counts the finished frame's below-horizon pixels no surface reached.
void wwCellFarLodSkyCensus( Scene * scene );

#endif // CELLFARLOD_H
