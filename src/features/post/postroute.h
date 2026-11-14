#pragma once
/* Lane VKFAR2 P7: cellpost.cpp's Vulkan route for the depth of field, motion blur and object blur, without Qt so the
 * offline gate (tests/vkframe/toneview.cpp, POST route arm) runs the same code: why the passes stay on GL before the
 * queue is tried, and the route lines "vk dof:", "vk motionblur:", "vk objblur:" postVkSay prints. */
#include "features/vkframe/vkpresent.h"

#include <cstdio>
#include <string>
#include <vector>

namespace WwPostRoute
{
// why GL runs the passes this frame without asking the queue (cellpost.cpp's order); nullptr: try tonePostQueue
inline const char * whyNotVk( bool red, bool linear, bool dump, bool pending, bool isView, bool d24s8, bool rg32 )
{
	if ( red )
		return "red WW_VKVIEW_POST_RED=1";
	if ( linear )
		return "the linear frame (before the tone map)";
	if ( dump )
		return "a dump frame (GL's targets are read back)";
	if ( !pending )
		return "no deferred tone pass (the view is not on Vulkan, or a GL pass crossed first)";
	if ( !isView )
		return "the frame is not the view";
	if ( !d24s8 )
		return "the frame's depth is not D24S8";
	if ( rg32 )
		return "red WW_CELL_MBLUR_RED=rg32 (RG32F vectors)";
	return nullptr;
}

// the route lines for the passes on this frame, from the stats summed since the last lines
inline std::vector<std::string> lines( bool vk, bool dof, bool lens, bool mb, bool ob, const WwVkPresent::FxStat & sum )
{
	char buf[512];
	std::string tail;
	if ( sum.runs > 0 ) {
		std::snprintf( buf, sizeof( buf ), "in the deferred tone pass %d frames (%d pipelines built in %.1f ms; %d vector copies; drawing "
			"thread %.3f ms, max %.3f)", sum.runs, sum.pipelines, sum.buildMs, sum.copies, sum.cpuMs / sum.runs, sum.maxMs );
		tail = buf;
	} else {
		tail = "0 frames in the tone pass";
	}
	if ( sum.gl > 0 )
		tail += ", on GL " + std::to_string( sum.gl ) + " frames (last: " + sum.glWhy + ")";
	if ( sum.fails > 0 )
		tail += ", refused " + std::to_string( sum.fails ) + " (last: " + sum.failWhy + ")";
	const std::string route = vk ? "on Vulkan" : "on GL";
	std::vector<std::string> out;
	if ( dof )
		out.push_back( "vk dof: " + route + ( lens ? " (the thin lens), " : " (the game's passes), " ) + tail );
	if ( mb )
		out.push_back( "vk motionblur: " + route + ( dof ? " (with the depth of field), " : " (alone), " ) + tail );
	if ( ob )
		out.push_back( "vk objblur: " + route + ", " + tail );
	return out;
}
}  // namespace WwPostRoute
