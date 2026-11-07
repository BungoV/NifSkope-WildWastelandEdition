#ifndef WW_AIRLAMPATLAS_H
#define WW_AIRLAMPATLAS_H

/* lane MODULES1 route (a) (notes/modules1/RESEARCH_AIR.md section 5): THE AIR'S LAMP SHADOWS FROM ONE DEPTH ATLAS.
 * The froxels asked each lamp's visibility by the probe walk (cell_volair.frag airLeakLightVis, 21.4 ms at the Museum).
 * The atlas holds six depth tiles a lamp (one cube, unfolded), the static world drawn once a cell, a few faces a frame,
 * and the air reads one 2x2 compare in place of the walk. WW_CELL_AIR_LAMPVIS = atlas | rays | walk (live, per frame);
 * walk is the old path (the red until bungo judges the pairs), rays is route (b) and reads the walk until it lands. */

#include "gl/glcontext.hpp"

#include <vector>

class Scene;
class Shape;
class Renderer;
struct WwCellLighting;

//! a caster of the light cubes (celllights.cpp, the same filter as its cube pass): world sphere
struct WwCubeCaster
{
	const Shape * sh;
	float c[3];
	float rad;
};

// celllights.cpp (owner of the cube pass's caster rules and draw)
void wwCellCubeCasters( Scene * scene, std::vector<WwCubeCaster> & out );
//! instances > 1: one instanced draw, one instance a cube face (the layered program cell_shadowdepth6.prog)
void wwCellCubeDrawCaster( Renderer * r, NifSkopeOpenGLContext::Program * prog, const Shape * sh, int instances = 1 );

//! 0 walk, 1 atlas, 2 rays (the pin WW_CELL_AIR_LAMPVIS, read live, else the Volumetric Air > Lamp shadows row)
int wwAirLampVisMode();
bool wwAirLampVisPinned();
//! the row's item: 0 Atlas, 1 Ray traced, 2 Probe walk
int wwAirLampVisChoice();
void wwAirLampVisSetChoice( int item );
//! the atlas's red, read live (WW_CELL_AIR_LAMPATLAS_RED): 1 "twice" = lamps in a cube slot read the atlas too,
//! 2 "faces" = the first fill capped at 192 faces a frame, not 4 ms, 4 "perface" = one draw a face (the old fill), not
//! one instanced draw a caster for its six faces
int wwAirLampAtlasRed();
//! part 22 red WW_AIR_ATLAS_ALL_RED=1: lamps without the shadow flag get atlas tiles and air shadows too (before part 22)
bool wwAirAtlasAllRed();

/* one frame of the atlas: tiles for the lamps whose sphere reaches the air (camera distance - radius < farD), drawn
 * nearest first, binds the atlas on unit AirLampAtlas and the table on AirLampTable. Returns the lamps in the table
 * (0 = nothing bound: the air reads the walk). Leaves the GL state as it found it, except the program (none bound). */
int wwAirLampAtlasFrame( Scene * scene, const WwCellLighting * L, const float cam[3], float p11, int viewH, float farD );

#endif
