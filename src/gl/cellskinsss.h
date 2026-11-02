#pragma once
// lane SKINSSS1: screen-space skin subsurface scattering in the cell view (row "Skin subsurface", WW_CELL_SKINSSS,
// ships off). The opaque pass (celllights.cpp) tags skin on its attachment 1; this pass blurs the skin's light with
// FO4CS's separable law (src/gl/wwskinsss.h, res/shaders/cell_skinsss.frag) in the linear frame, before the post
// chain. Red WW_CELL_SKINSSS_RED=1: the passes run and the composite keeps the frame.

class Scene;

//! the row is on (the loader's define is in the programs) and the scene is a cell view
bool wwCellSkinSssWanted( Scene * scene );
//! extract, blur x, blur y + composite into the bound linear frame (no GL call while the row is off)
void wwCellSkinSssApply( Scene * scene );
