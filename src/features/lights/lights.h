/* lane MODULES1 (lights): the lights module's interface: light shadows, flicker, XEMI lights, the light spheres.
 * The cell light law itself (WwCellLight, the light buffer, the uniforms) stays in gl/celllights.h. */
#ifndef WW_FEATURES_LIGHTS_H
#define WW_FEATURES_LIGHTS_H

#include <QString>
#include <QtGlobal>

#include <vector>

class Scene;

/*! Rows for light shadows (WW/CellShadow, WW_CELL_SHADOW) and GPU relight (WW/CellGiGpu, WW_CELL_GI_GPU, read when
 *  a cell opens). Each env pin wins over its row. The AO rows: src/features/ao/ao.h. */
bool wwCellShadowOn();
void wwCellShadowSetOn( bool on );

/*! LIGHT FLICKER (lane LIGHTFLICKER1): a placed LIGH with flag 0x8 (flicker) or 0x80 / 0x100 (pulse) varies each frame as
 *  the game does (measured; celllights.cpp wwCellFlickerFrame): a dimmer on its colour and an
 *  offset of its position. One value per light per frame, written into the light buffer that the surfaces, the effects
 *  and the air all read, so they always agree. The bake (GI, relight, far light) keeps the steady light.
 *  The Light flicker row ships off (WW/CellFlicker; the pin WW_CELL_FLICKER wins). WW_CELL_FLICKER_RED=1 holds every
 *  light steady (dimmer 1, no offset: the row-off picture). WW_CELL_FLICKER_STEP=<seconds> a fixed step a frame (gates),
 *  WW_CELL_FLICKER_DUMP=<file> one line per flickering light per frame. */
bool wwCellFlickerOn();
void wwCellFlickerSetOn( bool on );
//! once a frame, before the draw: advance the scene's document's flickering lights by the frame's time
void wwCellFlickerFrame( Scene * scene );
//! the view should repaint every frame: the row on and not red, cell-lit, the document has a flickering light
bool wwCellFlickerWantsFrames( Scene * scene );
//! light i's value this frame (dimmer on its colour, world offset); false (1, 0) when it is steady
bool wwCellFlickerOf( const void * nif, qsizetype i, float * dimmer, float offset[3] );
/*! lane LIGHTS4 (XEMI-LIGHT): once a frame, the colour factor of each light whose placement has XEMI: its source's
 *  colour^2.2 when its row (Placement / Region emittance) is on, else 1. A change rebuilds the light buffer. */
void wwCellXemiLightsFrame( Scene * scene );

//! lane VOLAIR1: the uploaded light buffer as spheres (world x, y, z, radius per light, the buffer's order, copies
//! included; cellLightCount of them), or null when it is not this scene's; valid after wwCellLightsUniforms
const std::vector<float> * wwCellLightSpheres( Scene * scene );

/*! Shadows: each shadow-casting light near the camera (up to kShadowSlots) gets a depth cube rendered from the
 *  document's opaque shapes, re-rendered only when the document, lights or slots change. WW_CELL_SHADOW=0 skips it. */
void wwCellShadowPass( Scene * scene );
//! lane DOORLIVE1: a caster moved (a live door): the next pass re-renders every held shadow slot
void wwCellShadowKick( const void * nif );
//! lane LIVESET1: a section of the document was rebuilt live: every held slot and the lamp store start over
void wwCellShadowsForget( const void * nif );
QString wwCellShadowEcho( Scene * scene );

#endif
