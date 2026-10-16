#ifndef CELLFXLIT_H
#define CELLFXLIT_H

#include <QString>
#include <QVector>

#include <cstddef>

class Scene;
struct WwCellLight;

/*! Placed lights on a lit effect (effect-lighting flag): the game multiplies its color by
 *  mix(1, directional + up to four placed lights x fade x pow(1 - saturate(d / r)^2, 2.2), lighting influence),
 *  per pixel, no N.L or shadow. The four lights belong to the placed MODEL; the renderer asks this table per draw.
 *  WW_CELL_FXLIT_PICK=strong, WW_CELL_FXLIT_RED=white|nofade|all|nopower, WW_CELL_FXLIT_DUMP=<file>: debug aids. */
struct WwFxLitModel
{
	float center[3] = { 0, 0, 0 };  //!< the placed model's bounding sphere, world
	float radius = 0.0f;
	quint32 ref = 0;                //!< the placement's form
	QString model;
	int light[4] = { -1, -1, -1, -1 };          //!< indices into WwCellLighting::lights, -1 = none
	float scale[4] = { 1, 1, 1, 1 };            //!< 1, or the nofade red control's ratio
};

/*! The game's light pick: candidates reach the model's bound (distance - bound radius < light radius); under a
 *  150-unit bound they sort by (distance - bound r) / light r, largest first, else record order (inferred); the
 *  first four win. `strongest` instead takes the four largest color x falloff at the bound's center. */
void wwCellFxLitPickLights( const QVector<WwCellLight> & lights, const float center[3], float radius,
	bool strongest, int out[4] );

//! A new scene for this document: its table is emptied.
void wwCellFxLitBegin( const void * nif );
//! One placed model with a lit effect shape; returns its serial.
int wwCellFxLitModel( const void * nif, const WwFxLitModel & m );
//! The written shape `block` belongs to model `serial`.
void wwCellFxLitShape( const void * nif, int block, int serial );
//! Red control data: light `index` of the published list at its base's fade is `ratio` x what was published.
void wwCellFxLitNoOffset( const void * nif, int index, float ratio );
//! After the document's lights are published: pick every model's lights (and write the dump).
void wwCellFxLitPick( const void * nif );
//! The model a shape belongs to, or null (not a lit effect of this document).
const WwFxLitModel * wwCellFxLitFor( const void * nif, int block, int * serial = nullptr );
//! Called with the effect program bound for `block`: the shape's lights (fxLit, fxLitScale, fxLitMode, fxLitId).
void wwCellFxLitUniforms( Scene * scene, int block );
//! True when this shape draws in the running probe pass (a lit effect, probe 70..74).
bool wwCellFxLitProbeShape( Scene * scene, int block );
/*! True when this exterior lit effect draws with fxRoomLit: its base scales by the open-air light reaching it, so a
 *  lens in a dark shop follows the room. WW_CELL_FXROOM_RED=game keeps the game's base. */
bool wwCellFxRoomLit( Scene * scene, int block );

//! Bounding sphere of `count` points (xyz, stride 3 floats): the box centre, the farthest point.
void wwCellFxLitSphere( const float * xyz, size_t count, float out[4] );
//! Grow sphere `a` (xyz, radius; radius < 0 = empty) to hold sphere `b`, as a scene node merges its children's.
void wwCellFxLitMerge( float a[4], const float b[4] );

#endif
