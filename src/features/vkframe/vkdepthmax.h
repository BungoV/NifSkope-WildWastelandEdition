#pragma once
/* lane VKSMALL1 (2026-11-16): the frame's farthest depth per pixel (cellhdr.cpp depthMaxMake, cell_depthmax) on the
 * present device, behind WW_VKVIEW=1 (red WW_VKVIEW_DEPTHMAX_RED=1 keeps GL's quad). The source is the shared tone frame's
 * own depth (read in place through a depth-aspect view); the target is a shared R32F twin the route owns, so GL's readers
 * (the far cards, the volume clouds, the underwater fog) take its GL name. One full-screen draw, one crossing. */
#include "features/vkframe/vkpresent.h"

#include <string>

namespace WwVkPresent
{
// depthmax_view_ps's b31 (cell_depthmax_ps.hlsl: one int)
inline constexpr FxField kT_depthmax[] = { { "depthSamples", 0, 1, 1, 1, 0 } };
inline constexpr int kT_depthmaxBytes = 16;
struct DepthMaxRun
{
	int w = 0, h = 0;      //!< the frame (the shared tone frame's size)
	unsigned ds = 0;       //!< GL's multisample depth-stencil the draw reads (must be the tone frame's)
	int samples = 0;       //!< depthSamples: the frame's samples
	int red = 0;           //!< gates only: 1 depthSamples 1 (sample 0 alone), 2 the target cleared, no draw
};
/* GL current: the farthest depth drawn on Vulkan into the route's shared R32F twin; tex = its GL name (NEAREST, CLAMP).
 * false: why; nothing drawn (GL draws its quad into its own target). */
bool depthMaxRun( const DepthMaxRun & r, unsigned & tex, std::string & why );
void depthMaxOnGl( const char * why );
FxStat takeDepthMaxStat();
}  // namespace WwVkPresent
