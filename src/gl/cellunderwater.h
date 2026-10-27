#ifndef CELLUNDERWATER_H
#define CELLUNDERWATER_H

#include <QString>

class Scene;
struct WwWaterRecord;

/*! Lane WATERUNDER1 (2026-10-27, WATER1's owed C5): THE CAMERA UNDER WATER.
 *
 *  When the cell-view camera is below a water surface, one full-screen pass over the linear frame (cellhdr.h, before
 *  its tone map) draws the WATR's underwater fog: amount x saturate((L - near) / (far - near)) over the view distance L
 *  (to the surface when the ray leaves the water), its colour half the water sky's horizon + half the underwater
 *  colour (the game's surface-from-below variant, so the two meet), and a darkening by depth below the surface (the
 *  underwater colour as a filter over `far` units, the probe bake's rule). res/shaders/water_under.glsl is the model.
 *  Lane WATERUNDER3: the game's law (read from its image-space depth-of-field shader, no capture): the same linear
 *  ramp over the VIEW DEPTH (planar), not the ray length; here too (red WW_CELL_UNDERWATER_RED=radial).
 *
 *  Which water: the baked body at the camera (cellwaterinfo.h, its WATR form), else the lowest registered water shape
 *  above it (cellwater.h). The waterline splits per pixel (each pixel's near-plane point against the surface).
 *  Lane WATERUNDER5: that shape's polygon and mean plane, not under it in a room below the water; the worldspace's
 *  default water reads as sea. The fog colour = the WATR colour x the game's WaterColorMult (white indoors without
 *  Show Sky, else the weather's Water Multiplier row).
 *  The Underwater fog row (WwLook::Underwater, pin WW_CELL_UNDERWATER), default off. Off, or the camera above water:
 *  no GL call. Harness only: WW_CELL_UNDERWATER_RED=notint (no depth darkening) | nofog (no view fog) | oldplane
 *  (box + top vertex, no room test) | oldcolour (fog colour = mean of horizon and WATR colour).
 *
 *  Lane WATERLOOK1 step 16 (WATERUNDER2): the Underwater light row (WwLook::WaterUnderLight, pin
 *  WW_CELL_WATER_UNDERLIGHT, default off) runs this pass by itself or with the fog row. Under water it takes the
 *  Water clear colour's coefficients for the body's class in place of the WATR fog (res/shaders/
 *  water_underlight.glsl): transmission exp(-k L), single scattering of the sun and the sky each dimmed with depth,
 *  the scene dimmed by the light's way down. At the waterline a dark meniscus band 1.5 px each side. The water
 *  program draws Snell's window on the surface seen from below (cellwater.h). The sun is the cell's surfaces' sun
 *  (the weather sun x Sunlight Scale outdoors), the sky the water's sky. Harness only:
 *  WW_CELL_WATER_UNDERLIGHT_RED=nomeniscus | nocolor (the fog law stays).
 *
 *  Lane WATERUNDER3: with the Underwater light row and the eye under water, the frame's imagespace takes the WATR's
 *  INAM imagespace (the game's underwater base data): its HDR tone-map fields, cinematic and tint; the frame keeps its
 *  LUT and the surfaces' Sunlight/Sky Scale. The volume clouds (drawn after the tone map) are left out while the eye
 *  is under water: the fog covers the far plane. Harness only: WW_CELL_WATER_UNDERLIGHT_RED=noimagespace | clouds |
 *  oldsun (the surface from below takes the viewport light again). */

//! Before the scene draws, every frame: is the camera's eye under water (decides the imagespace for this frame).
void wwCellUnderwaterFrame( Scene * scene );
//! The water record whose INAM imagespace this frame takes (the eye under water, the light row on), else nullptr.
const WwWaterRecord * wwCellUnderwaterImageSpace( const void * nif );
//! This frame's pass fogged an eye under water: the volume clouds stay out.
bool wwCellUnderwaterHidesClouds();
//! The sun under water: the cell surfaces' sun (the weather sun x Sunlight Scale outdoors, else the cell's
//! directional), `to` normalized world, colour 0 below the horizon. One source for the pass and the water program.
bool wwCellUnderwaterSun( Scene * scene, float to[3], float col[3] );
//! Inside the linear frame, after the scene and before wwCellHdrEnd: the fog when the camera is under water.
void wwCellUnderwaterPass( Scene * scene );
//! One line for the notes: what the last frame found (water, height, record, fog) or why it drew nothing.
QString wwCellUnderwaterEcho();

#endif
