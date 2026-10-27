#ifndef CELLWATER_H
#define CELLWATER_H

#include "cellblockmap.h"

#include <QString>

#include <string_view>

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
 *    WW_CELL_WATER_RED=<term>   norefl | nofresnel | nosilt | nospec | noshore | nonormal: that term left out;
 *                               lane WATER2: nodepthfog (the second pass), nofar (the noise sampler's anisotropy),
 *                               nossr (the reflections computed, not read), fogunits (that pass's distances in metres)
 *    WW_CELL_WATER_PROBE=<n>    raw values instead of the colour (res/shaders/fo4_water.frag lists them)
 *    WW_CELL_WATER_DUMP=<file>  the records, the sky and the sun the draw used, one line each */

//! A new scene for this document: its table is emptied.
void wwCellWaterBegin( const void * nif );
//! The written shape `block` is a water surface of record `rec`.
void wwCellWaterShape( const void * nif, int block, const WwWaterRecord & rec );
// lane CELLSTREAM1: a streamed cell let go of this block (it is about to be reused)
void wwCellWaterForget( const void * nif, int block );
//! lane LIVESET3: the document's blocks were renumbered (a live set removed or moved some)
void wwCellWaterRemap( const void * nif, const WwBlockMap & m );
//! True when `block` draws as water this frame (the row on, not pinned off, a registered water shape).
bool wwCellWaterWanted( Scene * scene, int block );
//! Lane SUNCELL1: `block` is a registered water surface (row or not): it never casts a sun shadow, as in the game.
bool wwCellWaterIsShape( const void * nif, int block );
//! lane WATERLOOK1: the document registered any water shape (the ocean pass builds only then)
bool wwCellWaterHasShapes( const void * nif );
//! lane WATERLOOK1: the shore foam's drive (the weather's wind as 1-20 m/s, 5 with none; the sea swell in m)
void wwCellWaterDrive( float & windMs, float & swellM );
//! lane CELLALL1: the Water row (WW/CellWater, default on as before); the pin WW_CELL_WATER wins
bool wwCellWaterOn();
void wwCellWaterSetOn( bool on );
//! With fo4_water.prog bound for `block`: the record, the sky, the textures. False = draw it the old way.
bool wwCellWaterUniforms( Scene * scene, int block, int firstTextureUnit );
//! WATERHDR1: the water just set up writes linear light into the cell view's HDR frame (the renderer marks its
//! pixels for the one tone map, cellhdr.h). False for its probes and its reflection-ray pass (raw data).
bool wwCellWaterWritesLinear();
//! Lane WATER2: a water surface of this scene draws as water and its WATR's SSR flag is set (cellssr.h).
bool wwCellWaterSsrWanted( Scene * scene );
/*! lane WATERCOMBINE1 (2026-10-27): every water look row below draws through ONE twin, fo4_waterlook.prog (all four
 *  defines compiled in), and each look reads its own uniform (waterDepthOn, waterCauOn, waterShoreOn bits 1 edge and
 *  2 foam, waterWavesOn), so any set of ticks draws all of them together. All rows off = fo4_water.prog, its machine
 *  code unchanged. Probes 32/33 depth, 34 swell, 35/36 caustics, 37 shore (fo4_water.frag's header).
 *  lane WATERDEPTH1 (2026-10-27): the Water depth colour row (WwLook::WaterDepth, pin WW_CELL_WATER_DEPTH):
 *  the depth colour from the baked bed depth (res/shaders/water_depth.glsl) where the frame has
 *  no scene behind the water, pure water's absorption (Pope & Fry 1997) on the game's ramp, and the far colour
 *  letting the body through by 1 - F. Off = fo4_water.prog as before. Red WW_CELL_WATER_DEPTH_RED=flat | noabsorb |
 *  nearest; probes WW_CELL_WATER_PROBE=32 (the depth used) and 33 (who wins). */
/*! lane WATERCAUSTICS1 (2026-10-27): the Water caustics row (WwLook::WaterCaustics, pin WW_CELL_WATER_CAUSTICS):
 *  the sun's caustics on the drawn bed
 *  under a baked water body (res/shaders/water_caustics.glsl), on the world clock, faded by depth, pure water's
 *  absorption and the sun's cascades. Red WW_CELL_WATER_CAUSTICS_RED=nofade | noshadow | still | flat | lattice
 *  (CAUSTICPATTERN1: the old four-sine law); probes WW_CELL_WATER_PROBE=35 (the factor / 2) and 36 (the net's E / 8,
 *  envelope, sun share). CAUSTICPATTERN1 (10-27): the pattern is a warped, animated Voronoi net of thin filaments,
 *  two octaves, the bed's mean light kept (notes causticpattern1). */
/*! lane WATERLOOK1 step 8 (WATERCOLOR1, 2026-10-27): the Water clear colour row (WwLook::WaterClear, pin
 *  WW_CELL_WATER_CLEAR): the water's own absorption and lit in-scatter by body class in place of the WATR's depth ramp
 *  and silt (res/shaders/water_clear.glsl): the clear sea turquoise over sand at 1-3 m, blue past about 10 m; lakes
 *  greener, rivers brown-green. Red WW_CELL_WATER_CLEAR_RED=noscatter | nodown | fast. */
/*! lane WATERLOOK1 step 10 (WATERSKYVIS1, 2026-10-27): the Water sky visibility row (WwLook::WaterSkyVis, pin
 *  WW_CELL_WATER_SKYVIS): the water's sky reflection read on the probe sky grid's open-sky share along the reflected
 *  lobe, the sun's glare along the sun (res/shaders/water_skyvis.glsl): water under a bridge or a pier is darker than
 *  open water at the same angle. The row packs the sky grid into cellGi as SPECOCC1 does (celllights.cpp).
 *  Red WW_CELL_WATER_SKYVIS_RED=one (visibility 1) | nosun. */
const char * wwCellWaterProgram();
//! lane WATERCOMBINE1: any water look row is on (the water draws with fo4_waterlook.prog)
bool wwCellWaterLookOn();
//! any water program (the looks' twin or the default)
inline bool wwIsCellWaterProgramName( std::string_view n )
{
	return n == "fo4_water.prog" || n == "fo4_waterlook.prog" || n == "fo4_waterlookd.prog";
}
//! One census line: how many shapes and records this document registered.
QString wwCellWaterEcho( const void * nif );
/*! Lane WATERSHORE1: the Water soft edge and Shore foam rows: res/shaders/water_shore.glsl in fo4_waterlook.prog,
 *  read from the baked shore plane (cellwaterinfo.h), near and far alike.
 *    WW_CELL_WATER_EDGE=0|1, WW_CELL_WATER_FOAM=0|1   the rows' pins (celllooks.h)
 *    WW_CELL_WATER_SHORE_RED=nearest                  both read the nearest 128 u texel only (the staircase)
 *    WW_CELL_WATER_PROBE=37                           (edge, foam, shore distance / 1024) at each water pixel
 *                                                     (WATERCOMBINE1: was 32, which the depth probe holds) */
//! Lane WATERUNDER1: the registered record of WATR `form` in this document (else its first one); false = none.
bool wwCellWaterRecordOf( const void * nif, quint32 form, WwWaterRecord & rec );
/*! Lane WATERUNDER1: the lowest registered water surface above world (x, y, z) whose bounds hold (x, y) and that is
 *  at most `maxDepth` above z: its height and record. The bounds (shape verts x world transform) are cached per
 *  document and re-measured when its water shapes change. */
bool wwCellWaterPlaneOver( Scene * scene, float x, float y, float z, float maxDepth, float & height, WwWaterRecord & rec );

#endif
