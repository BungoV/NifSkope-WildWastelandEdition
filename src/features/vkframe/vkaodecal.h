#pragma once
// Lane VKAO3 (2026-11-16): the baked AO decals (cellaodecalgl.cpp wwCellAoDecalRun, cell_aodecal.prog) on the present device.
// GL's pass, draw for draw: per copy the mask (aoMode 0 into selfId, LEQUAL with polygon offset (-1, -4), stencil bit 0x80
// set) and the AO (aoMode 1 into target, cull FRONT, depth clamp, stencil NOTEQUAL 0x80, blend ONE ONE or DST_COLOR ZERO),
// then the bit cleared. The targets are shared (sharedTarget keys); the g-buffer and the models' volumes are GL's own and are
// copied into route-owned shared twins. Gate: tests/vkframe/aodecaldraw.sh. Code: vkaodecal.inc (included by vkpresent.cpp).
#define WW_VK_AODECAL 1
#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{
struct AoDecalCopy
{
	float clip[4][4], M[3][4], N[3][3], lo[3], hi[3], c[3], coef[9], fade, rcut, self;
	int dims[3];
	unsigned vol;  //!< GL 3D RGBA32F, dims.x x dims.y x 3 dims.z
	unsigned vbo;  //!< vec3 positions: maskVerts mask triangles, then the footprint box's 36
	int maskVerts;
	std::uint64_t volGen = 0;  //!< > 0: the owner's model-set generation (the volume and vertices copied only when it moves)
};

struct AoDecalRun
{
	int w = 0, h = 0;
	unsigned gbuf = 0;  //!< GL RGBA32F 2D (view normal, linear depth), copied into a route-owned twin every run
	bool add = false;   //!< aoAdd: blend ONE ONE; else DST_COLOR ZERO
	int red = 0;        //!< 1 mask dropped, 2 the other blend, 4 aoMode 0 in the AO draw, 8 cull BACK, 16 no clamp, 32 bit kept
	std::uintptr_t ds = 0, target = 0, selfId = 0;  //!< sharedTarget keys: D24S8, R32F, R32F
	float proj[4] = {}, sc = 1.0f;
	std::vector<AoDecalCopy> copies;
};

struct AoDecalStat
{
	int runs = 0, copies = 0, draws = 0, twinCopies = 0, fails = 0, gl = 0, pipelines = 0;
	double cpuMs = 0.0, maxMs = 0.0, buildMs = 0.0;
	std::string failWhy, glWhy;
};

//! empty when the route can run in this context; else why not
std::string aoDecalRefusal();
//! the whole pass on Vulkan (GL waits it); false = nothing drawn, why says so (GL draws)
bool aoDecalRun( const AoDecalRun & r, std::string & why );
//! counts a GL-drawn pass (why: the refusal)
void aoDecalOnGl( const char * why );
//! the counters since the last take
AoDecalStat takeAoDecalStat();
}  // namespace WwVkPresent
