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
	bool dumpFixed = false;                // read back each probe's 32 fixed distances (out.fixedDist)
};

struct DdgiGpuOut
{
	bool ok = false;
	std::string why;                       // empty when ok; else why the GPU did not run (the caller then runs the CPU twin)
	std::vector<float> probes;             // 4 a probe: offset xyz, state
	std::vector<uint32_t> age;             // updates blended a probe
	std::vector<float> irr;                // 8 x 8 x 4 a probe (encoded rgb, sky visibility)
	std::vector<float> dist;               // 16 x 16 x 2 a probe
	std::vector<float> fixedDist;          // 32 a probe, the stored distances of its fixed rays (job.dumpFixed only)
	double msBuild = 0, msReloc = 0, msFrames = 0;
	long long rays = 0;                    // probe rays traced: every probe's fixed rays per relocation / classification
	                                       // pass, then the ACTIVE probes' (not Off, not skipped Asleep) N per update
	double blasGrow = 0.0;                 // how far bakert grew each BLAS edge (0 = the true triangles; the trace re-tests)
	long long blasGrowTris = 0;
	std::string device;
};

//! the whole run (build, relocation, classification, frames, read back) on the bake device; false + why when it cannot
//! (M2: open, steps of up to 16 updates, the host copies, close)
bool ddgiGpuRun( const DdgiGpuJob & job, DdgiGpuOut * out );

/* M2 (2026-10-29): a volume kept on the device and updated a few times a frame (the cell view, src/rtddgicell.cpp).
 * The working buffers are device local; each step copies the tiles to host buffers (cached when the device has such
 * memory) that the caller reads until the next step. A step takes the device's lock only if it is free (a bake in
 * flight makes it `busy`, nothing run). Not thread-safe per session: one caller at a time. */
struct DdgiGpuSession;

//! builds the scene and the buffers, relocates and classifies as ddgiGpuRun does; out gets the states (probes), the
//! build and relocation ms, the BLAS growth and the device; nullptr + out->why when it cannot (job.frames unused)
DdgiGpuSession * ddgiGpuOpen( const DdgiGpuJob & job, DdgiGpuOut * out );

struct DdgiGpuStepOut
{
	bool ok = false, busy = false;
	std::string why;
	double ms[4] = { -1, -1, -1, -1 };    // GPU ms summed over the step's updates: trace, blend, border; [3] the copy to the host (-1: no timestamps)
	double msWall = 0;                    // the step on the CPU's clock, submit to fence
	long long rays = 0;
};
//! k updates (rotations: 9 floats each, row major, k of them), numbered frame0..; then the host copies (k = 0: the
//! copies alone). copyRays adds the rays buffer (the fixed rays' distances, ddgiGpuRun's dumpFixed).
bool ddgiGpuStep( DdgiGpuSession * s, const float * rotations, int k, uint32_t frame0, bool copyRays, DdgiGpuStepOut * out );
//! the host copies of the last step (nullptr before the first): irr 64 vec4 a probe, dist 256 vec2, age 1, probes 4
//! (offset xyz, state; written at open), rays N vec4 (copyRays only)
const float * ddgiGpuHostIrr( const DdgiGpuSession * s );
const float * ddgiGpuHostDist( const DdgiGpuSession * s );
const uint32_t * ddgiGpuHostAge( const DdgiGpuSession * s );
const float * ddgiGpuHostProbes( const DdgiGpuSession * s );
const float * ddgiGpuHostRays( const DdgiGpuSession * s );
//! frees everything (waits for the device's lock)
void ddgiGpuClose( DdgiGpuSession * s );

} // namespace bakert

#endif // RTDDGI_GPU_H
