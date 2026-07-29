/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef BAKEGEOM_H
#define BAKEGEOM_H

#include "data/niftypes.h"

#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVector>

class NifModel;

//! @file bakegeom.h Write captured effect geometry into a NIF as static shapes.
/*!
 * Particle sprites and procedural-lightning ribbons have NO geometry in the file.
 * A particle is one point that `particles.geom` expands into a camera-facing quad
 * on the GPU; a bolt is a polyline that `regenerate()` invents from the clock.
 * Both exist only in the frame being drawn.
 *
 * That is why a loading screen cannot simply carry them: 0 of the 173 files under
 * `meshes/LoadScreenArt` contain `NiParticleSystem`, `NiPSys*` or
 * `BSProceduralLightningController`. Keeping the effect means capturing what it
 * looks like at one instant and writing that out as an ordinary `BSTriShape` —
 * which is what this does.
 *
 * Two consequences the caller has to accept, not work around:
 *
 *   - **A still cannot turn.** The quads and ribbons are billboards; baking picks
 *     one facing and they are wrong from every other angle. `Capture::facing` is
 *     that choice, and a loading screen has a fixed camera, so it is a fair one.
 *   - **Particles integrate.** Sprite positions depend on every frame before
 *     them, so the scene must be STEPPED to the wanted instant, not jumped there.
 *     Arcs do not have this problem: `regenerate()` is seeded from quantised
 *     time, so one instant always reproduces one bolt.
 *
 * The vertex layout is the one the OBJ importer has been shipping (see
 * `obj.cpp`): full-precision position, half UV, byte normal/tangent, byte colour.
 * Full precision is deliberate — half floats step ~0.0078 units at Z ≈ 111 and
 * these are thin ribbons.
 */

namespace BakeGeom
{

//! One captured effect, ready to be written out.
struct Capture
{
	QString name;
	//! World space, 3 per triangle.
	QVector<Vector3> tris;
	//! One per vertex of `tris`.
	QVector<Vector2> uvs;
	//! One per vertex; carries the head/tail fade. May be empty.
	QVector<Color4> cols;
	Color4 tint = Color4( 1.0f, 1.0f, 1.0f, 1.0f );
	//! The direction the billboard was flattened against.
	Vector3 facing = Vector3( 0.0f, -1.0f, 0.0f );
	//! Blocks in the SAME model, duplicated onto the new shape.
	QModelIndex shaderProperty;
	QModelIndex alphaProperty;
	bool fromParticles = false;
};

struct Result
{
	bool ok = false;
	int shapes = 0;
	int vertices = 0;
	int triangles = 0;
	//! Emitter blocks removed once their look was captured.
	int emittersRemoved = 0;
	//! The name each capture was actually written under, aligned to the input.
	/*! Empty where that capture failed. Not always the requested name: an
	 *  assembled rig carries several effects with the SAME node name, so a
	 *  collision gets a suffix, and a caller that needs to find its shape again
	 *  has to be told which name it got. */
	QStringList shapeNames;
	//! Anything refused or approximated, said out loud.
	QStringList notes;
};

//! Write one capture into `nif` as a `BSTriShape` under the root.
/*! Returns the new block, or an invalid index with `error` set. Nothing is
 *  removed: the emitters are still there afterwards. */
QModelIndex writeShape( NifModel * nif, const Capture & cap, QString * error = nullptr );

//! Write every capture, then remove the emitters they replace.
/*! One undoable snapshot. `removeEmitters` false leaves the live effects in
 *  place alongside the bake, which is only useful for looking at both at once. */
Result write( NifModel * nif, const QVector<Capture> & caps, bool removeEmitters = true,
              QString * error = nullptr );

//! Remove particle systems and/or procedural lightning without baking anything.
/*! The "drop it" arm of the same choice. Returns the number of blocks removed. */
int dropEffects( NifModel * nif, bool particles, bool lightning );

//! Does this file contain anything a bake or a drop would act on?
bool hasEffects( const NifModel * nif, bool * particles = nullptr, bool * lightning = nullptr );

} // namespace BakeGeom

#endif // BAKEGEOM_H
