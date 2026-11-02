#ifndef CELLPRECIP_H
#define CELLPRECIP_H

/* lane WEATHERLOOK1 phase B (2026-10-30): precipitation in the cell view, drawn from the Lookdev weather.
 *
 * Source, per weather: its .recx block "precipitation" (docs/RECX.md; parser gl/precipspec.h), else its own record (WTHR
 * DATA class flags: Rainy -> a rain layer, Snow -> a snow_fine layer). Each layer: a type (rain, hail, snow_fine,
 * snow_far_dot, snow_wet_clump, snow_big_clump), its rate or density, size, draw scale and texture (under Textures\,
 * looked up loose in the .recx roots: the loaded plugins' folders, the game's Data, the shipped Data; each type ships a
 * default, res/Data/Textures/FO4CS/Precip, the phase A atlases of notes/weatherlook1; a texture under the old
 * WW/Precip is still found there and the other way round).
 * The field (gl/precipmath.h, unit tests/precip): a camera-centred box of world-anchored particles that fall at their
 * type's speed and ride the weather's wind (the fog drift's law and bearing); near ones are textured quads flat-on to
 * the camera with a slow spin, stretched over the shutter; far ones (out to 6 x the near box) are dots of the atlas
 * cell's mean colour, each standing for many. A weather change: the new weather's layers at pct, the old at 1 - pct.
 * Lit by the volumetric particles' law (cell_volpart.glsl vpLight), seen through the air, into the linear frame
 * (stencil 1, so they bloom and tone-map with the cell). None indoors.
 * Row: Show > Improved Sky > Precipitation (WW/CellPrecip), ships OFF; pin WW_CELL_PRECIP=0|1 (live). Harness pins:
 * WW_CELL_PRECIP_TIME=<s> (the field's clock; unset = the wall clock). Reds WW_CELL_PRECIP_RED=constdensity, nowrap,
 * onespeed, nowind (comma list; precipmath.h Red). Census on stderr: "precip: ...".
 * Lane PRECIPLOOK1 (2026-11-02): WW_CELL_PRECIP_LOOK=rain,snow,clump,hail (or 1/on/all; default off) draws the new look
 * on the GPU field (cell_precip_look.prog): rain as a clear water lens swept over the shutter, snow with its normal map
 * (thin/thick split, lamp glints), wet and big clumps from one density atlas at two thresholds, hail from the 3D build.
 * Red WW_CELL_PRECIP_LOOK_RED=1 = the old path. */

#include <QString>

class Scene;

//! inside the linear frame, after the opaque scene and the far cards: the particles; returns how many were drawn
int wwCellPrecipDraw( Scene * scene );
//! the view repaints while precipitation falls (never with the clock pinned)
bool wwCellPrecipWantsFrames( Scene * scene );
//! the last census line ("precip: off" when the row is off)
QString wwCellPrecipEcho();

/* lane FO76WEATHER1: rain wetness on every material (gl/rainwetlaw.h, res/shaders/rain_wet.glsl), Fallout 76's law.
 * The amount runs on Fallout 4's clock (+0.03 a second while it rains, -0.02 while dry) toward the rain's share of the
 * weather (a change blends both weathers); it starts settled when a document opens or the row turns on. Interiors 0.
 * Wet ground also takes the game's wet reflection (SSR) flag (renderer.cpp, src/landlodspec.h landSsr), and a
 * material with its wetness SSR switch takes the flag while wet.
 * Row: Show > Improved Sky > Rain wetness (WW/CellRainWet), ships OFF; pin WW_CELL_RAINWET=0|1 (live).
 * Harness pin WW_CELL_RAINWET_AMOUNT=<0..1> (a fixed amount, outdoors). Red WW_CELL_RAINWET_RED=shore: the old path
 * (shore wetness only, no rain wetness, dry SSR flags). Census on stderr: "rain wet: ...".
 * Lane FO76WEATHER2: a top-down cover map (gl/rainwetlaw.h exposed) keeps ground under roofs and bridges dry: solid
 * shapes without the terrain, 8192 u around the eye at 16 u texels, unit WwCellUnit::RainCover. Red
 * WW_CELL_RAINWET_RED=nocover: no cover test (FO76WEATHER1). Census "rain wet: cover map ...". */
//! the amount this frame (0 off, red, indoors or dry)
float wwCellRainWetAmount( Scene * scene );
//! the current program's rainWet uniform; custom porosity from the material (BGSM bCustomPorosity / fPorosityValue)
void wwCellRainWetUniforms( Scene * scene, float customPorosity, bool useCustom );
QString wwCellRainWetEcho();

#endif // CELLPRECIP_H
