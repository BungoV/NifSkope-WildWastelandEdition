#pragma once
// Lane VKAO4 (2026-11-16): the GI volume mask (ao.cpp giVolMaskPass, cell_givol.prog) on the present device. GL's pass, state
// for state: the R32F volMask cleared 0 on the opaque D24S8 depth, one instanced draw of every shape's hull (36 vertices from
// gl_VertexID, the shape from gl_InstanceID), depth GEQUAL without writes, cull FRONT, depth clamp, blend MAX. The target and the
// depth are shared (sharedTarget keys); the g-buffer is GL's own and is copied into a route-owned shared twin every run; the
// shapes (12 floats a shape, GiVolumeShape's layout) go into a uniform texel buffer. Gate: tests/vkframe/volmaskdraw.sh.
// Code: vkvolmask.inc (included by vkpresent.cpp).
#define WW_VK_VOLMASK 1
#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{
struct VolMaskRun
{
	int w = 0, h = 0;
	unsigned gbuf = 0;                     //!< GL RGBA32F 2D (view normal, linear depth), copied into a route-owned twin every run
	std::uintptr_t ds = 0, target = 0;     //!< sharedTarget keys: the opaque D24S8, the R32F volMask
	std::vector<float> shapes;             //!< 12 floats a shape: (centre, type), (size, room), (rotation degrees, 0)
	float clip[4][4] = {}, row[3][4] = {};  //!< volClip (world -> clip rows), volRow (view -> world rows)
	float bias = 0.0f, grow = 0.0f, proj[2] = {};
	int red = 0;  //!< 1 blend ADD, 2 depth LESS, 4 n - 1 instances, 8 no clamp, 16 cull BACK, 32 volGrow 0
};

struct VolMaskStat
{
	int runs = 0, draws = 0, shapes = 0, twinCopies = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! empty when the route can run in this context; else why not
std::string volMaskRefusal();
//! the whole pass on Vulkan (GL waits it); false = nothing drawn, why says so (GL draws)
bool volMaskRun( const VolMaskRun & r, std::string & why );
//! counts a GL-drawn pass (why: the refusal)
void volMaskOnGl( const char * why );
//! the counters since the last take
VolMaskStat takeVolMaskStat();
}  // namespace WwVkPresent
