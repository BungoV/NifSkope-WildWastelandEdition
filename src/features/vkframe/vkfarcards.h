#pragma once
/* lane VKFAR1 round 2 (row G3, 2026-11-13): the far cards' (impostor_cellinstez) inputs on the present device
 * (vkfarcards.inc, built into vkpresent.cpp). Nothing draws the cards there yet: these are the pieces their pass needs. */
#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{
struct FarTwinStat
{
	int made = 0, dropped = 0, fails = 0;
	double mb = 0.0;
	std::string failWhy;
};
/*! a twin sharedTarget does not make: type 0 2D, 2 2D array (layers), 3 cube-map array (layers = 6 x cubes);
 * glFmt a BC name (S3TC / RGTC / BPTC, sRGB too), GL_DEPTH_COMPONENT16, or one of sharedTarget's colour formats.
 * GL draws / copies into tex as into its own texture (GL_TEXTURE_2D, _2D_ARRAY, _CUBE_MAP_ARRAY); the present device
 * samples the same memory. Keyed like sharedTarget (its own map); refused without textureCompressionBC / imageCubeArray. */
bool farTwin( std::uintptr_t key, int type, int w, int h, int layers, unsigned glFmt, unsigned & tex, std::string & why, int levels = 1 );
void farTwinDrop( std::uintptr_t key );
//! one level of one layer as the present device sees it (packed blocks / texels); for the gates
bool farTwinRead( std::uintptr_t key, int level, int layer, std::vector<std::uint8_t> & out, std::string & why );
FarTwinStat farTwinStat();
void farCardsFree( bool glAlive );  //!< shutdown(): everything above goes with the view
}  // namespace WwVkPresent
