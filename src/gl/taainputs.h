#ifndef TAAINPUTS_H
#define TAAINPUTS_H

#include <QString>
#include <QVector>

class Node;
class Scene;

/*! Lane TAAINPUTS1 (2026-10-23): INPUTS for a temporal resolve and the motion blur, produced, never consumed here.
 *
 *  - velocity (RG16F): previous minus current in texture units, unjittered (the game_taa_mv convention). Every pixel
 *    the camera's reprojection of the frame's depth; where a thing moved on its own (skinned shapes, a world
 *    transform that moved, the cell's grass wind) its own vector, drawn from last frame's matrices / skin / wind phase.
 *  - mask (RGBA8): R reactive (particles, effect shapes, alpha-blended lit shapes: their alpha), G composition (water,
 *    alpha-blended lit glass: 1), B = 1 where the velocity is an object vector.
 *  Runs only when a consumer asked (wwTaaInputsRequest, sticky 2 passes), the debug view is on, or WW_TAAINPUTS=1;
 *  otherwise not one GL call, so the frame is the before-lane frame. notes/taainputs1/INTERFACE.md is the contract. */

//! A consumer will read the inputs (call each frame it is on; the first frame after it gets none).
void wwTaaInputsRequest();
//! The scene's blended pass, as drawn this frame (Scene::drawDeferredShapes): the mask's shapes.
void wwTaaInputsNoteBlended( Scene * scene, const QVector<Node *> & drawn );
//! After the frame's draw, before the depth of field / motion blur / temporal AA: the two passes.
void wwTaaInputsPass( Scene * scene );
//! After the temporal AA: the debug view (WW_TAAINPUTS_VIEW) over the finished frame; no GL call when off.
void wwTaaInputsView( Scene * scene );
//! This frame's textures (0 when not produced this frame) and their size.
unsigned wwTaaInputsVelocityTex( Scene * scene, int & w, int & h );
unsigned wwTaaInputsMaskTex( Scene * scene, int & w, int & h );
//! One line: what the last pass drew and cost, or why it did not run.
QString wwTaaInputsEcho();

#endif
