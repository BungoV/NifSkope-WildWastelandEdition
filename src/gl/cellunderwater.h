#ifndef CELLUNDERWATER_H
#define CELLUNDERWATER_H

#include <QString>

class Scene;

/*! Lane WATERUNDER1 (2026-10-27, WATER1's owed C5): THE CAMERA UNDER WATER.
 *
 *  When the cell-view camera is below a water surface, one full-screen pass over the linear frame (cellhdr.h, before
 *  its tone map) draws the WATR's underwater fog: amount x saturate((L - near) / (far - near)) over the view distance L
 *  (to the surface when the ray leaves the water), its colour half the water sky's horizon + half the underwater
 *  colour (the game's surface-from-below variant, so the two meet), and a darkening by depth below the surface (the
 *  underwater colour as a filter over `far` units, the probe bake's rule). res/shaders/water_under.glsl is the model.
 *  The game's own use of uwFog is uncaptured (WATER3 RESEARCH item 2): INFERRED. INAM is not read (unknown).
 *
 *  Which water: the baked body at the camera (cellwaterinfo.h, its WATR form), else the lowest registered water shape
 *  above it (cellwater.h). The waterline splits per pixel (each pixel's near-plane point against the surface).
 *  The Underwater fog row (WwLook::Underwater, pin WW_CELL_UNDERWATER), default off. Off, or the camera above water:
 *  no GL call. Harness only: WW_CELL_UNDERWATER_RED=notint (no depth darkening) | nofog (no view fog). */

//! Inside the linear frame, after the scene and before wwCellHdrEnd: the fog when the camera is under water.
void wwCellUnderwaterPass( Scene * scene );
//! One line for the notes: what the last frame found (water, height, record, fog) or why it drew nothing.
QString wwCellUnderwaterEcho();

#endif
