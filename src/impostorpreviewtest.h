/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTORPREVIEWTEST_H
#define IMPOSTORPREVIEWTEST_H

class GLView;
class NifSkope;
class QWidget;
class Scene;

// `WW_IMPOSTOR_PREVIEW` -- the octahedral impostor preview, and its harness.

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

//! Read `WW_IMPOSTOR_PREVIEW` and, when it is set, arm the harness on `skope`.
bool wwImpostorPreviewStart( NifSkope * skope, GLView * ogl, QWidget * viewportHeader );

#endif // IMPOSTORPREVIEWTEST_H
