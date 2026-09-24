/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTORPREVIEWTEST_H
#define IMPOSTORPREVIEWTEST_H

class GLView;
class NifSkope;
class QWidget;
class Scene;

/* ---------------------------------------------------------------------------
 * `WW_IMPOSTOR_PREVIEW` -- the octahedral impostor preview, and its harness.
 *
 * FOUR functions, because a shared file should have to learn as little as
 * possible about this lane. Three of them are one-line hunks in `glview.cpp`
 * and `nifskope_ui.cpp`; everything else is in `impostorpreviewtest.cpp`.
 *
 * The same two drawing entry points serve BOTH readers of this lane:
 *   - the harness, which measures and quits;
 *   - the viewer feature, which draws a `.lodm` a person opened.
 * They are not two code paths that could drift apart -- the number the gate
 * prints comes out of the pass the person is looking at.
 * ------------------------------------------------------------------------- */

//! Is a card being previewed at all? False in every ordinary session: the
//! preview is armed by opening a `<id>_oct.lodm` or by the harness, and the
//! chunk-placement half is a feature master that ships OFF.
bool wwImpostorPreviewActive();

//! Should the scene's own shapes be skipped for this frame? True only while
//! the harness is photographing the CARD alone, so that the card's silhouette
//! is measured against the mesh's and not against both drawn together.
bool wwImpostorPreviewSuppressScene();

//! Draw the armed card into the frame being painted. Does nothing, and costs
//! one predictable-branch, when nothing is armed. Call it after the scene's
//! opaque pass and before the transparent one: a card writes depth.
void wwImpostorPreviewDraw( Scene * scene );

/*! Read `WW_IMPOSTOR_PREVIEW` and, when it is set, arm the harness on `skope`.
 *
 *  Call it once at startup, beside the other WW_* harnesses. Returns true when
 *  the harness took the session over -- the caller should then leave the window
 *  alone, because the harness owns the camera, the window size and the exit.
 *
 *  `ogl` and `viewportHeader` are passed IN rather than reached for: they are
 *  private members of NifSkope, and this lane is not widening that class's
 *  interface for a harness. The call site has them; one line hands them over.
 */
bool wwImpostorPreviewStart( NifSkope * skope, GLView * ogl, QWidget * viewportHeader );

#endif // IMPOSTORPREVIEWTEST_H
