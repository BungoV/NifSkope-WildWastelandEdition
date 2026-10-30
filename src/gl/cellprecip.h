#ifndef CELLPRECIP_H
#define CELLPRECIP_H

/* lane WEATHERLOOK1 phase B (2026-10-30): precipitation in the cell view, drawn from the Lookdev weather.
 *
 * Source, per weather: its .recx block "precipitation" (docs/RECX.md; parser gl/precipspec.h), else its own record (WTHR
 * DATA class flags: Rainy -> a rain layer, Snow -> a snow_fine layer). Each layer: a type (rain, sleet, hail, snow_fine,
 * snow_far_dot, snow_wet_clump, snow_big_clump), its rate or density, size, draw scale and texture (under Textures\,
 * looked up loose in the .recx roots: the loaded plugins' folders, the game's Data, the shipped Data; each type ships a
 * default, res/Data/Textures/WW/Precip, the phase A atlases of notes/weatherlook1).
 * The field (gl/precipmath.h, unit tests/precip): a camera-centred box of world-anchored particles that fall at their
 * type's speed and ride the weather's wind (the fog drift's law and bearing); near ones are textured quads flat-on to
 * the camera with a slow spin, stretched over the shutter; far ones (out to 6 x the near box) are dots of the atlas
 * cell's mean colour, each standing for many. A weather change: the new weather's layers at pct, the old at 1 - pct.
 * Lit by the volumetric particles' law (cell_volpart.glsl vpLight), seen through the air, into the linear frame
 * (stencil 1, so they bloom and tone-map with the cell). None indoors.
 * Row: Show > Improved Sky > Precipitation (WW/CellPrecip), ships OFF; pin WW_CELL_PRECIP=0|1 (live). Harness pins:
 * WW_CELL_PRECIP_TIME=<s> (the field's clock; unset = the wall clock). Reds WW_CELL_PRECIP_RED=constdensity, nowrap,
 * onespeed, nowind (comma list; precipmath.h Red). Census on stderr: "precip: ...". */

#include <QString>

class Scene;

//! inside the linear frame, after the opaque scene and the far cards: the particles; returns how many were drawn
int wwCellPrecipDraw( Scene * scene );
//! the view repaints while precipitation falls (never with the clock pinned)
bool wwCellPrecipWantsFrames( Scene * scene );
//! the last census line ("precip: off" when the row is off)
QString wwCellPrecipEcho();

#endif // CELLPRECIP_H
