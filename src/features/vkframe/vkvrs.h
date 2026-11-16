#pragma once
/* lane VKSMALL3 (2026-11-16): the next frame's shading rate images (cellvrs.cpp wwCellVrsBuild, cell_vrs_rate) on the
 * present device, behind WW_VKVIEW=1 (red WW_VKVIEW_VRS_RED=1 keeps GL's blit + quad). The source is the view GL just
 * drew (its multisample RGBA8 colour, resolved on Vulkan into a twin the route owns, as GL's blit resolved it); the
 * targets are two shared R8UI twins (opaque, sorted) at the tile grid's size. GL copies them into its own rate images
 * (the NV binding stays GL's). One resolve + one full-screen draw, one crossing. */
#include "features/vkframe/vkpresent.h"

#include <string>

namespace WwVkPresent
{
// vrs_rate_view_ps's b31 (cell_vrs_rate_ps.hlsl; spirv-cross --reflect of the shipped SPIR-V)
inline constexpr FxField kT_vrs[] = {
	{ "vrsMvOn", 0, 1, 1, 1, 0 }, { "vrsSize", 4, 2, 1, 0, 0 }, { "vrsTile", 16, 2, 1, 0, 0 },
	{ "vrsCut", 24, 1, 1, 0, 0 }, { "vrsCutSorted", 28, 1, 1, 0, 0 }, { "vrsMax", 32, 1, 1, 1, 0 },
	{ "vrsMotion", 36, 1, 1, 0, 0 },
};
inline constexpr int kT_vrsBytes = 48;
struct VrsRun
{
	int w = 0, h = 0;          //!< the picture (the view's size)
	unsigned drawFb = 0;       //!< GL's draw framebuffer the picture is in (must be the view's, viewFbo())
	int tw = 0, th = 0;        //!< the rate images' size (tiles)
	int texelW = 16, texelH = 16;
	unsigned mv = 0;           //!< vrsMv when vrsMvOn: GL's RG32F vectors at the picture's size (0: off)
	float cut = 0.0f, cutSorted = 0.0f, motion = 0.0f;
	int maxRate = 2;
	int red = 0;               //!< gates only: 1 vrsMax 2, 2 the targets cleared, no draw
};
/* GL current: both rate images drawn on Vulkan into the route's shared R8UI twins; tex[0] opaque, tex[1] sorted (GL
 * names, tw x th). false: why; nothing drawn (GL blits and draws its own). */
bool vrsRun( const VrsRun & r, unsigned tex[2], std::string & why );
void vrsOnGl( const char * why );
FxStat takeVrsStat();
}  // namespace WwVkPresent
