#pragma once

/* Lane VKWATER4 (2026-11-14, plan row L7): the camera-underwater fog (cell_underwater.prog + its per-sample twin
 * cell_underwater_ms.prog) on the Vulkan view. Behind WW_VKVIEW=1, red WW_VKVIEW_UNDERWATER_RED=1 keeps GL's draws.
 * Both twins read only their loose block (b31), uwDepth (GL's R32F farthest depth per pixel, cellhdr.cpp
 * wwCellHdrDepthMax: copied into a shared twin every run) and uwDepthMs (the tone frame's own depth per sample, read in
 * place). The waterInfo / waterFlow images they declare are never read (notes/vkwater4/tools/spv_used.py), so nothing of
 * the water-info tables is shared. One crossing: the per-pixel draw, then the per-sample one, blended
 * ( ONE, SRC1_COLOR, ZERO, ONE ) into the tone frame's colour at its samples. */

#include <cstdint>
#include <string>

#include "features/vkframe/vkwater_tables.h"

namespace WwVkPresent
{

struct UnderRun
{
	int w = 0, h = 0;              //!< the frame (the shared tone frame's size)
	unsigned color = 0;            //!< the draw target's colour: the shared tone frame's
	unsigned depth = 0;            //!< uwDepth: GL's R32F farthest depth per pixel
	int depthW = 0, depthH = 0;
	unsigned depthFmt = 0;         //!< its internal format (GL_R32F only)
	unsigned depthMs = 0;          //!< uwDepthMs: the multisample depth-stencil bound for the per-sample reads
	int samples = 0;               //!< uwSamples: the frame's samples (0: GL's one draw from the farthest depth)
	int vp[4] = {};                //!< GL's viewport
	std::uint8_t loose[2][kT_waterUwBytes] = {};  //!< b31 of cell_underwater, of cell_underwater_ms (fxPack)
	int red = 0;  //!< gates only: 1 no per-sample draw, 2 blend without SRC1, 4 the uwDepth copy skipped, 8 uwRow one vec4 off
};
//! GL current, GL's state set as for its draws: both draws run on Vulkan. false: why (GL draws them)
bool underRun( const UnderRun & r, std::string & why );
//! what underRun cannot take on this device (empty: none)
std::string underRefusal();
void underOnGl( const char * why );
FxStat takeUnderStat();
//! the fog's route line ("vk under:"), one a 5 s window while on Vulkan (not in WW_VKPRESENT_NO_QT builds)
void underSay( bool wanted, const char * glWhy );

//! the loose block GL's program holds now (both programs share the table: water_tables.py reflects the same 27 fields)
template <class Fn> void underPack( Fn * fn, unsigned prog, std::uint8_t * out )
{
	fxPack( fn, prog, kT_waterUw, int( sizeof( kT_waterUw ) / sizeof( kT_waterUw[0] ) ), out );
}

}  // namespace WwVkPresent
