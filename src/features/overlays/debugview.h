#pragma once

#include <QStringList>

class Scene;
class Node;

/*! lane DEBUGVIEW1: the live debug views. One switch, WW_DEBUG_VIEW=<name or id> (live: a set reloads the shaders),
 *  and the Show > Debug view menu (Off first). A view replaces each pixel with one pass or term of the cell programs
 *  (fo4_default, pbrm_default, the far tree cards, the water); a term a program does not have reads black. The view
 *  turns the frame into a probe pass (celllights.cpp wwCellProbePass): no linear frame, no effects, no post, the
 *  clear black. Off: no loader define, no uniform, no GL call: the shipping path byte for byte. */
//! The view's id (0 off): the env pin when set, else the menu's
int wwDebugViewId();
//! The loader's WW_DEBUG_VIEWS (glcontext.cpp Shader::load): on while a view is on
bool wwDebugViewShaderDefine();
//! The menu's choice (the env pin, when set, wins)
void wwDebugViewSet( int id );
bool wwDebugViewPinned();
//! The menu's names, Off first, in id order
QStringList wwDebugViewNames();
/*! Who drew the shape, for the farowner view: 0 near object, 1 near land, 2 far terrain, 3 far objects, 6 far water
 *  (the walk of renderer.cpp wwLandLodMode over the "FarLOD*" parents) */
int wwDebugViewOwner( Scene * scene, const Node * mesh );
//! Once a frame (glview.cpp): `debug view: <name> (owner pass list)` when the view changes
void wwDebugViewFrame();
