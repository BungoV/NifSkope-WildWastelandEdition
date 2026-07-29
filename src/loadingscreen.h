/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LOADINGSCREEN_H
#define LOADINGSCREEN_H

#include <QString>
#include <QStringList>

class NifModel;

//! @file loadingscreen.h Turn a posed, assembled rig into loading-screen art.
/*!
 * The format is not a guess. Fallout 4 ships the exact thing this makes —
 * `meshes/LoadScreenArt/Armor03PowerArmor8X01.nif`, 25 blocks — and the whole
 * corpus under `meshes/LoadScreenArt` is 173 files, which answers every design
 * question that one file would have got wrong:
 *
 *   - Every armour piece is a plain `BSTriShape`: no `BSSkin::Instance`, no
 *     `BoneData`. **The skin is evaluated away, not carried.**
 *   - No `NiNode`s except the root and an optional `LoadingMenuZoomTarget`
 *     (65 of 173). **The skeleton is dropped.**
 *   - Each shape has a non-zero translation and an IDENTITY rotation, and its
 *     vertices sit near a local origin. That is not a style choice: FO4 vertices
 *     are half floats, where the step at Z ≈ 111 is ~0.0078 units — baking world
 *     coordinates straight in puts visible faceting on a helmet. **Per shape,
 *     translation = the piece's world centroid, vertices = world − centroid.**
 *   - Controllers are NOT required to be stripped — 18 of 173 files move and 13
 *     animate a shader. Freezing is a separate, optional step (see freezeanim.h).
 *   - `NiPSys*`, `NiParticleSystem` and `BSProceduralLightningController` appear
 *     **0 times in 173 files.** That is not proof the engine refuses them, but it
 *     is the whole of the evidence.
 */

namespace LoadingScreen
{

struct Result
{
	bool ok = false;
	//! Skinned shapes evaluated into static ones.
	int shapesBaked = 0;
	//! Rigid shapes whose parent-chain transform was folded in.
	int shapesFolded = 0;
	int nodesRemoved = 0;
	int blocksRemoved = 0;
	bool zoomTargetAdded = false;
	//! Anything dropped or left behind, said out loud.
	QStringList notes;
};

//! Bake the file as it currently stands into loading-screen art.
/*!
 * Runs as one undoable snapshot operation. Whatever pose the bones are in when
 * this is called is the pose that gets baked — that is the point: assemble,
 * pose, freeze each effect at its own moment, look at it, then convert.
 *
 * \param addZoomTarget   Add the `LoadingMenuZoomTarget` node the camera focuses
 *                        on, at the centre of what was baked.
 * \param keepParticles   Keep particle systems and procedural lightning. They
 *                        have no vanilla precedent whatsoever, so the default
 *                        drops them and says so; this is the override for
 *                        someone who would rather test it than lose the effect.
 */
Result convert( NifModel * nif, bool addZoomTarget = true, bool keepParticles = false,
                QString * error = nullptr );

} // namespace LoadingScreen

#endif // LOADINGSCREEN_H
