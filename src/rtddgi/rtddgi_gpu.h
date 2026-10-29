#ifndef RTDDGI_GPU_H
#define RTDDGI_GPU_H

/* Lane RTXGI1 (2026-10-29): the DDGI volume's GPU run on bakert's Vulkan device (ray query). No Qt. The code is
 * src/rtddgi/rtddgi_vk.inc, compiled inside src/bakert.cpp's translation unit (it reuses the device, the buffer
 * helpers and the BLAS/TLAS + alpha-mask build there). src/rtddgi.cpp fills the job; the CPU twin there runs the
 * same steps with the same packed inputs, so the two can be compared texel for texel. */

#include <cstdint>
#include <string>
#include <vector>

namespace probebvh { struct Bvh; }

namespace bakert {

//! the shaders' Params block (src/rtddgi/ddgi_common.glsl), std430, 16 vec4
struct DdgiGpuParams
{
	int32_t dims[4] = { 0, 0, 0, 0 };     // probes x y z, total
	float origin[4] = { 0, 0, 0, 0 };     // first probe (the soup's local frame), w = distClamp
	float stepv[4] = { 0, 0, 0, 0 };      // spacing per axis, w = the smallest
	float sunTo[4] = { 0, 0, 1, 0 };      // w = 1 on
	float sun[4] = { 0, 0, 0, 0 };        // w = trace tmax
	float dirTo[4] = { 0, 0, 1, 0 };      // w = 1 on
	float dirColor[4] = { 0, 0, 0, 0 };   // w = fixtureClear
	float sky[6][4] = {};                 // radiance cube; sky[0][3] = 1 on
	int32_t counts[4] = { 0, 0, 0, 0 };   // rays, lights, soup triangles, opaque triangles (the runner fills z, w)
	int32_t flags[4] = { 0, 0, 0, 0 };    // oneBounce, skipAsleep, useMask (the runner fills it), red bits
	float blendp[4] = { 0, 0, 0, 0 };     // hysteresis, sharpness, self-shadow bias B, 0
};
static_assert( sizeof( DdgiGpuParams ) == 16 * 16, "DdgiGpuParams must match the GLSL block" );

struct DdgiGpuJob
{
	const probebvh::Bvh * bvh = nullptr;   // the soup in its local frame, alpha masks set (bakert builds the BLAS from it)
	DdgiGpuParams params;
	std::vector<float> triAttr;            // 8 floats a soup triangle: albedo rgb, two-sided, Le rgb, 0
	std::vector<float> lights;             // 32 floats a light (src/rtddgi.cpp packLights)
	std::vector<float> offsets;            // 3 a probe: the start offsets (0 = on the grid)
	std::vector<float> rotations;          // 9 a frame (row major): the updates' ray rotations
	int frames = 0;
	bool relocate = true, classify = true;
};

struct DdgiGpuOut
{
	bool ok = false;
	std::string why;                       // empty when ok; else why the GPU did not run (the caller then runs the CPU twin)
	std::vector<float> probes;             // 4 a probe: offset xyz, state
	std::vector<uint32_t> age;             // updates blended a probe
	std::vector<float> irr;                // 8 x 8 x 4 a probe (encoded rgb, 1)
	std::vector<float> dist;               // 16 x 16 x 2 a probe
	double msBuild = 0, msReloc = 0, msFrames = 0;
	long long rays = 0;                    // probe rays dispatched (fixed + update)
	std::string device;
};

//! the whole run (build, relocation, classification, frames, read back) on the bake device; false + why when it cannot
bool ddgiGpuRun( const DdgiGpuJob & job, DdgiGpuOut * out );

} // namespace bakert

#endif // RTDDGI_GPU_H
