#ifndef CELLWATER_H
#define CELLWATER_H

#include <QString>

class Scene;
struct WwWaterRecord;

/*! Lane WATER1 (2026-10-04): THE GAME'S WATER IN THE CELL VIEW.
 *
 *  The cell's water planes (XCWT, else the world's default) and the placed water meshes (an ACTI with a WNAM)
 *  are written as their own shapes (src/cellview.cpp) and registered here with their WATR record
 *  (src/esmwater.h, the one reader). While the Cell lights row draws, such a shape draws in the second pass
 *  through fo4_water.prog, the game's near-water pixel shader term for term:
 *    three noise normals (world xy / UV scale, amplitude-weighted, the 2nd and 3rd faded by the noise falloff,
 *    all flattened toward the shore), Fresnel F0 + (1 - F0)(1 - N.V)^5, the reflection = the game's 3-colour
 *    water sky (horizon, lower, upper by the reflected ray's z) from the weather rows the dome uses, the
 *    refraction = the scene behind (the renderer's copy) silt-tinted over the depth curve, the sun specular and
 *    the sparkle, the shore blend to the plain refraction, the far fade to the reflection colour, then the fog.
 *  Under the surface: the game's underwater variant (half sky + half underwater colour, mixed by 1 - F).
 *
 *  Part of the Cell lights row: no setting of its own. Harness only:
 *    WW_CELL_WATER=0            the water draws as before this lane (the off identity's pin)
 *    WW_CELL_WATER_RED=<term>   norefl | nofresnel | nosilt | nospec | noshore | nonormal: that term left out
 *    WW_CELL_WATER_PROBE=<n>    raw values instead of the colour (res/shaders/fo4_water.frag lists them)
 *    WW_CELL_WATER_DUMP=<file>  the records, the sky and the sun the draw used, one line each */

//! A new scene for this document: its table is emptied.
void wwCellWaterBegin( const void * nif );
//! The written shape `block` is a water surface of record `rec`.
void wwCellWaterShape( const void * nif, int block, const WwWaterRecord & rec );
//! True when `block` draws as water this frame (the row on, not pinned off, a registered water shape).
bool wwCellWaterWanted( Scene * scene, int block );
//! With fo4_water.prog bound for `block`: the record, the sky, the textures. False = draw it the old way.
bool wwCellWaterUniforms( Scene * scene, int block, int firstTextureUnit );
//! One census line: how many shapes and records this document registered.
QString wwCellWaterEcho( const void * nif );

#endif
