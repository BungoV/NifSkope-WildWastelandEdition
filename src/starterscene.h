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
 * Blender starts on a cube rather than on nothing, and for the same reason: an
 * empty document gives you nowhere to click. Most of what this editor can do
 * needs a shape to do it to — Add Primitive itself refuses without one, because
 * it clones an existing BSTriShape for its vertex layout and material.
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
 * Not the viewport's Add Primitive default of 1.0: that one is dropped at the 3D
 * cursor in a scene that already has something in it, with Size live in the
 * operator panel. A document containing nothing else has to be visible on its
 * own, and at 1.0 it is not — GLView::frameAll() snaps any scene whose radius is
 * below the unit scale to a 1024-unit sphere, so a 1-unit cube stays sub-pixel
 * however the camera is framed. Measured: at 1.0 the viewport is empty, at 100
 * the cube fills a third of it.
 */
constexpr float STARTER_CUBE_SIZE = 2.0f * 69.99125f;

/*! Replace a model's contents with a minimal Fallout 4 scene: an NiNode root
 * holding one cube BSTriShape with its own BSLightingShaderProperty and
 * BSShaderTextureSet.
 *
 * The shader property carries no material path and the texture set is empty, so
 * the cube renders unshaded black — the honest state for a shape with no
 * material, and the first thing to change by pointing the shader property's Name
 * at a .bgsm. (The same document opened from disk renders in the no-texture
 * magenta instead: a loaded file has a folder for texture lookup to fail
 * against, where a document that was never on disk has none. Cosmetic, and the
 * one loose end here.)
 */
bool nifCreateStarterScene( NifModel * nif, float size = STARTER_CUBE_SIZE,
							QString * error = nullptr );

#endif // STARTERSCENE_H
