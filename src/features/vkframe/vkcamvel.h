#pragma once

/* Lane VKOBJ18 (2026-11-16): the TAA inputs' step 2 (game_taa_mv.prog, the camera's vectors: a full-screen quad) on the present
 * device, into g.vel (RG16F) from g.depth, the targets wwTaaInputsPass shares while the view is on. Plain data, no Qt.
 * Included by vkpresent.cpp's vkcamvel.inc and by src/features/post/taainputs.cpp. */
#include <cstdint>
#include <string>

namespace WwVkPresent
{
struct CamVelRun
{
	float reproj[16] = {};                         //!< as glUniformMatrix4fv takes it (column-major)
	float jitterNdc[2] = {}, invSize[2] = {};
	std::uintptr_t velKey = 0, depthKey = 0;       //!< the sharedTarget keys of g.vel and g.depth
	int w = 0, h = 0;
	/* gate reds (tests/vkframe/camveldraw.cpp; the app passes 0): 1 the jitter dropped, 2 reproj written one row (16 bytes)
	 * late, 3 the depth slot reads the zero view */
	int red = 0;
};

struct CamVelStat
{
	int runs = 0, gl = 0, fails = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string glWhy, failWhy;
};

//! empty when the route can run in this context; else why not
std::string camVelRefusal();
//! the quad on Vulkan, one crossing (GL waits it); false: nothing drawn, why names the refusal
bool camVelRun( const CamVelRun & r, std::string & why );
//! the app drew the quad on GL (why: the refusal)
void camVelOnGl( const char * why );
CamVelStat takeCamVelStat();
}  // namespace WwVkPresent
