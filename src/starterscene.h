/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef STARTERSCENE_H
#define STARTERSCENE_H

#include <QString>

class NifModel;

/*! \file starterscene.h
 * \brief The document NifSkope opens with when no file was given.
 *
 * An EMPTY Fallout 4 document: the right header and one root NiNode, nothing
 * else. It used to open on a cube, on Blender's reasoning that an empty document
 * gives you nowhere to click; that turned out not to be worth the cost of a
 * shape nobody asked for in every new file, so the cube is gone.
 *
 * The root node stays, and it is not decoration. Everything that edits a
 * document reaches for a parent to put things under — a block dropped in the
 * block list, a merged Loaded NIF, the drop-replacement guard that identifies a
 * clean starter by editing block 0's name. A truly empty model has no block 0.
 *
 * Model layer only, so `NifSkope -no-gui new -o out.nif` writes the same
 * document the GUI opens with and it can be checked without a window.
 */

/*! Two metres in Fallout 4 units — the size of Blender's default cube.
 *
 * 69.99125 game units to the Havok metre is the constant the collision decoder
 * is validated against (hknpdecode.h, glnode.cpp), so this is that conversion
 * rather than a number picked to look right.
 *
 * Only the fixture cube uses it now; nothing in the GUI does.
 */
constexpr float STARTER_CUBE_SIZE = 2.0f * 69.99125f;

/*! Replace a model's contents with the starter document: a Fallout 4 header
 * (20.2.0.7, user 12, BS 130) and one empty root NiNode named "Scene Root".
 *
 * This is what a new window holds, what Reload gives back to a document that was
 * never on disk, and what `-no-gui new` writes.
 */
bool nifCreateStarterScene( NifModel * nif, QString * error = nullptr );

/*! Replace a model's contents with the starter document plus one cube
 * BSTriShape, with its own BSLightingShaderProperty and BSShaderTextureSet.
 *
 * A TEST FIXTURE, not a product path — `-no-gui new --cube` is the only caller,
 * and it exists because the harnesses that exercise the block list, renaming,
 * merging and collision need a small Fallout 4 scene with real geometry in it
 * and must not need a game corpus to get one. Add Primitive cannot supply it:
 * that spell clones an existing BSTriShape for its vertex layout and material,
 * so it refuses on a document that has no shape yet (glview.cpp).
 *
 * The shader property carries no material path, so the cube renders with the
 * shader's own defaults until one is chosen.
 */
bool nifCreateCubeScene( NifModel * nif, float size = STARTER_CUBE_SIZE,
						 QString * error = nullptr );

#endif // STARTERSCENE_H
