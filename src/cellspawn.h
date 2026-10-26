/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSPAWN_H
#define CELLSPAWN_H

#include <QString>
#include <QStringList>

class EsmWorld;
struct CellSceneSpec;

/*! lane CELLSPAWN1: where a freshly opened cell puts the camera.
 *  Exterior: the centre of the opened cell, standing on its LAND (or on the water when the water is higher), or the
 *  nearest of the cell's 33x33 terrain points no placed object stands on. Interior: the cell's COC marker, else the
 *  arrival point of its first load door. Not valid: the old framing (View > Center). */
struct CellSpawn
{
	bool valid = false;
	QString rule;               //!< which rule placed it, in words
	float eye[3] = { 0, 0, 0 }; //!< world units
	float yawDeg = 0.0f;        //!< 0 looks north (+y), 90 east (+x): a REFR's heading in degrees
	float ground = 0.0f;        //!< what the eye stands on (LAND, water, marker or arrival z)
	bool water = false;         //!< exterior: the cell has water
	float waterHeight = 0.0f;
	int blockers = 0;           //!< exterior: placed objects whose bounds stand on the centre
	float moved = 0.0f;         //!< exterior: how far the nearest open point is from the centre
	QString centreObjects;      //!< exterior: the first few objects standing on the centre (form, model, box size)
	QString line() const;       //!< the notes line, "cell spawn: ..."
};

/*! The player's eye above his feet: node 'Camera' of FO4's first-person skeleton
 *  (Meshes\Actors\Character\_1stPerson\CharacterAssets\skeleton.nif) stands at z 120.4828. */
constexpr float cellEyeHeight = 120.4828f;

//! The spawn for this spec, read from the world the open indexed. WW_CELL_SPAWN_RED=old|z0 are the gate's reds.
CellSpawn cellSpawnCompute( const EsmWorld & world, const CellSceneSpec & spec );
//! The open of document `nif` publishes its spawn; the window takes it once, on the tab's first load.
void cellSpawnPublish( const void * nif, const CellSpawn & s );
bool cellSpawnTake( const void * nif, CellSpawn & out );

//! `nifskope-cli cellspawn "<spec line>" [--check]`: the spawn without a window, and the eye-height check
int cellSpawnCli( const QStringList & args );

#endif // CELLSPAWN_H
