#ifndef RTDDGI_GPU_H
#define RTDDGI_GPU_H

/* Lane RTXGI1 (2026-10-29): the DDGI volume's GPU run on bakert's Vulkan device (ray query). No Qt. The code is
 * src/rtddgi/rtddgi_vk.inc, compiled inside src/bakert.cpp's translation unit (it reuses the device, the buffer
 * helpers and the BLAS/TLAS + alpha-mask build there). src/rtddgi.cpp fills the job; the CPU twin there runs the
 * same steps with the same packed inputs, so the two can be compared texel for texel. */

#include <cstdint>
#include <functional>
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
	/* RTXCASCADE2: the probes the updates skip (trace mode 0, blend, border): grid indices skipLo..skipHi inclusive per
	 * axis, skipLo[3] = 1 on (rtddgimath.h cascadeCovered: a coarse cascade's probes a finer one covers whole); 0 off */
	int32_t skipLo[4] = { 0, 0, 0, 0 };
	int32_t skipHi[4] = { -1, -1, -1, 0 };
};
static_assert( sizeof( DdgiGpuParams ) == 18 * 16, "DdgiGpuParams must match the GLSL block" );

struct DdgiGpuSession;

struct DdgiGpuJob
{
	/* RTXCASCADE2: open on this session's scene (its BLAS + TLAS, alpha masks, triangle attributes and lights: the same
	 * soup and lights in the same local frame; the caller keeps it open until ddgiGpuOpen returns); bvh then unused for
	 * the device (no BLAS is built) */
	DdgiGpuSession * sceneFrom = nullptr;
	const probebvh::Bvh * bvh = nullptr;   // the soup in its local frame, alpha masks set (bakert builds the BLAS from it)
	DdgiGpuParams params;
	std::vector<float> triAttr;            // 8 floats a soup triangle: albedo rgb, two-sided, Le rgb, 0
	std::vector<float> lights;             // 32 floats a light (src/rtddgi.cpp packLights)
	std::vector<float> offsets;            // 3 a probe: the start offsets (0 = on the grid)
	std::vector<float> rotations;          // 9 a frame (row major): the updates' ray rotations
	int frames = 0;
	bool relocate = true, classify = true;
	bool dumpFixed = false;                // read back each probe's 32 fixed distances (out.fixedDist)
	// M3 (sessions only): `share` makes the atlas exportable to GL (Win32 external memory + two semaphores) when the
	// device can; `scroll` keeps the scratch buffer a scroll moves the probes through
	bool share = false, scroll = false;
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
	bool blasBuilt = false, blasShared = false;   // RTXCASCADE2: this open built a BLAS / took job.sceneFrom's
};

//! the whole run (build, relocation, classification, frames, read back) on the bake device; false + why when it cannot
//! (M2: open, steps of up to 16 updates, the host copies, close)
bool ddgiGpuRun( const DdgiGpuJob & job, DdgiGpuOut * out );

/* M2 (2026-10-29): a volume kept on the device and updated a few times a frame (the cell view, src/rtddgicell.cpp).
 * The working buffers are device local; each step copies the tiles to host buffers (cached when the device has such
 * memory) that the caller reads until the next step. A step takes the device's lock only if it is free (a bake in
 * flight makes it `busy`, nothing run). Not thread-safe per session: one caller at a time.
 *
 * M3 (2026-10-29): irradiance, distance and the probes live in ONE device buffer, the atlas, in cell_ddgi.glsl's
 * layout (irr 64 vec4 a probe, then dist 128 vec4 a probe, then 1 vec4 a probe: offset xyz, state); the kernels bind
 * its three ranges. With job.share it is exported (VK_KHR_external_memory_win32) and GL reads it in place: no copy, no
 * upload. Two exported semaphores order the two APIs: the session signals `vkDone` at the end of every submit (GL waits
 * on it before it reads), GL signals `glDone` after its reads (the next submit waits on it before it writes). A step
 * may then return without waiting (in.wait false): the next step finds the submit done (or says busy) and reports it.
 * A step can also scroll the volume first (whole cells, the kept probes' data moved, the new planes reset and woken). */
struct DdgiGpuSession;

//! builds the scene and the buffers, relocates and classifies as ddgiGpuRun does; out gets the states (probes), the
//! build and relocation ms, the BLAS growth and the device; nullptr + out->why when it cannot (job.frames unused)
DdgiGpuSession * ddgiGpuOpen( const DdgiGpuJob & job, DdgiGpuOut * out );

struct DdgiGpuStepOut
{
	bool ok = false, busy = false;
	std::string why;
	/* the LAST FINISHED submit (with in.wait this step's; without, an earlier one, or none: done false):
	 * GPU ms summed over its updates: trace, blend, border; [3] the copies to the host; [4] its scroll (copies, resets,
	 * the fresh probes' relocation and classification); -1: no timestamps or none of it */
	bool done = false;
	double ms[5] = { -1, -1, -1, -1, -1 };
	bool hostNew = false;                 // it copied the tiles to the host buffers (ddgiGpuHostIrr/Dist/Age)
	bool probesNew = false;               // it was a scroll: `probes` holds the new states (4 a probe: offset xyz, state),
	std::vector<float> probes;            // copied at the harvest, before a later submit can write the host buffer again
	int updates = 0;                      // its updates
	// this call
	double msWall = 0;                    // on the CPU's clock: record + submit (+ the wait with in.wait)
	long long rays = 0;                   // the rays this call's updates trace (the active probes' N each)
	bool submitted = false, pending = false;   // a submit went in; it is still running (in.wait false)
};
//! k updates (rotations: 9 floats each, row major, k of them), numbered frame0..; then the host copies (k = 0: the
//! copies alone). copyRays adds the rays buffer (the fixed rays' distances, ddgiGpuRun's dumpFixed). Waits.
bool ddgiGpuStep( DdgiGpuSession * s, const float * rotations, int k, uint32_t frame0, bool copyRays, DdgiGpuStepOut * out );

//! M3: a step's whole order
struct DdgiGpuStepIn
{
	const float * rotations = nullptr;    // 9 floats an update, row major (nullptr: no rotation)
	int k = 0;                            // updates, 0..16
	uint32_t frame0 = 0;
	bool copyRays = false;
	bool host = true;                     // copy irr, dist, age to the host buffers (readback, a dump, the irradiance view)
	bool wait = true;                     // wait for the device; false: return after the submit
	/* scroll first: the first probe moves to `origin` (the soup's local frame, packVolume's), `runs` (old first probe, new
	 * first probe, count: rtddgimath.h scrollRuns) keep the kept probes' tiles, age, offset and state; every other probe
	 * is reset (zero tiles, age 0, offset 0, DDGI_FRESH), then relocated (5 passes) and classified, fresh ones only */
	bool scroll = false;
	float origin[3] = { 0, 0, 0 };
	std::vector<uint32_t> runs;
	/* interop: called with the device's lock held, the last submit finished and this one recorded, just before the
	 * submit; returns true when the caller has signalled glDone (the submit then waits on it before its first write) */
	std::function<bool()> beforeSubmit;
	/* RTXCASCADE2: skipSet = the session's skip range becomes skipLo / skipHi (DdgiGpuParams; written to the device only
	 * when it moved, before the scroll and the updates); false = left as it is (M3's steps never set it) */
	bool skipSet = false;
	int32_t skipLo[4] = { 0, 0, 0, 0 };
	int32_t skipHi[4] = { -1, -1, -1, 0 };
};
//! false + busy: the lock is taken or the last submit still runs (nothing recorded, beforeSubmit not called)
bool ddgiGpuStepEx( DdgiGpuSession * s, const DdgiGpuStepIn & in, DdgiGpuStepOut * out );
/*! RTXCASCADE2: one submit for n sessions on one scene (job.sceneFrom): each member's step recorded in order into one
 *  command buffer, one vkQueueSubmit (waits on every glDone a member's beforeSubmit signalled, signals every interop
 *  member's vkDone). False: not one scene (why), or busy (a member's last submit still runs, or the lock is taken:
 *  nothing recorded); members harvested before the refusal keep their reports in out[]. */
bool ddgiGpuStepGroup( DdgiGpuSession * const * s, const DdgiGpuStepIn * in, DdgiGpuStepOut * out, int n );
//! RTXCASCADE2: the two sessions trace one scene (one BLAS)
bool ddgiGpuSameScene( const DdgiGpuSession * a, const DdgiGpuSession * b );
//! RTXCASCADE2: the BLAS builds of DDGI sessions and the DDGI step submits since the process started (the CLI's census)
long long ddgiGpuBlasBuilds();
long long ddgiGpuSubmits();
//! waits for a submit still running and reports it as done (out->done false: there was none)
bool ddgiGpuFinish( DdgiGpuSession * s, DdgiGpuStepOut * out );

//! M3: the atlas exported to GL (job.share); every handle is a new NT handle the caller closes (ddgiGpuCloseHandle)
struct DdgiGpuShare
{
	bool ok = false;
	std::string why;                      // when not ok: why the atlas is not shared (the caller reads back)
	void * memory = nullptr;              // the atlas's device memory (GL_HANDLE_TYPE_OPAQUE_WIN32_EXT)
	uint64_t memorySize = 0;              // the allocation's size: GL imports exactly this
	uint64_t atlasBytes = 0;              // the atlas itself: probes x 193 x 16 bytes, at offset 0
	bool dedicated = true;                // a dedicated allocation (GL_DEDICATED_MEMORY_OBJECT_EXT)
	void * vkDone = nullptr, * glDone = nullptr;
	uint8_t deviceUuid[16] = {}, driverUuid[16] = {};
};
bool ddgiGpuShare( DdgiGpuSession * s, DdgiGpuShare * out );
void ddgiGpuCloseHandle( void * h );
//! the host copies of the last step (nullptr before the first): irr 64 vec4 a probe, dist 256 vec2, age 1, probes 4
//! (offset xyz, state; written at open), rays N vec4 (copyRays only)
const float * ddgiGpuHostIrr( const DdgiGpuSession * s );
const float * ddgiGpuHostDist( const DdgiGpuSession * s );
const uint32_t * ddgiGpuHostAge( const DdgiGpuSession * s );
const float * ddgiGpuHostProbes( const DdgiGpuSession * s );
const float * ddgiGpuHostRays( const DdgiGpuSession * s );
//! frees everything (waits for the device's lock and a submit still running)
void ddgiGpuClose( DdgiGpuSession * s );

/* Lane RESTIRDI1 (2026-10-30): ReSTIR DI for the shadow-casting lights, per pixel, on the volume's own BVH and packed
 * lights (src/rtddgi/restir.comp; the CPU twin is src/restirmath.h restir::frame). One step = the four passes on a
 * W x H g-buffer (view normal, linear depth; 1e6 none; bottom row first), then the copies that make this frame the next
 * one's history. Its own command buffer, fence and (shared) semaphores: the volume's steps are not disturbed. */
struct RestirGpuIn
{
	int W = 0, H = 0;
	uint32_t frame = 0;
	int red = 0;                          // restir_common.glsl RESTIR_BIT_* (novis, nopdf, notemporal, noclamp, nohole)
	int candidates = 8, spatial = 4, filter = 2;
	float radius = 16.0f, bias = 0.5f, clear = 0.0f;
	float rows[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };   // view -> the soup's local frame
	float proj[4] = { 1, 1, 0, 0 };                            // P00, P11, P20, P21
	float prows[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };  // the previous frame's
	float pproj[4] = { 1, 1, 0, 0 };
	bool history = false;                 // the previous frame's g-buffer, reservoirs and running means are valid
	std::vector<int> cand;                // the candidate lights (indices into the volume's packed list)
	const float * gbuf = nullptr;         // host g-buffer W x H x 4 (the CLI); nullptr: GL wrote the shared one
	bool readBack = false;                // copy the ratio, num / den / blocked, reservoirs and running means to the host
	std::function<bool()> beforeSubmit;   // interop (restirGpuShare): GL signalled glDone; the submit waits on it
};
struct RestirGpuOut
{
	bool ok = false, busy = false;
	std::string why;
	double ms = -1;                       // the four passes + copies on the device (-1: no timestamps)
	double msWall = 0;
	std::vector<float> ratio;             // readBack: 2 a pixel (ratio, linear depth)
	std::vector<float> numDen;            // 4 a pixel (num, den, blocked 0/1 (2: no ray), 0)
	std::vector<float> res;               // 4 a pixel (light as a float, wsum, M, W): the frame's output reservoir
	std::vector<float> acc;               // 4 a pixel (the running means: num, den, frames, 0)
};
//! false + busy when the device's lock is taken; waits for its own submit
bool restirGpuStep( DdgiGpuSession * s, const RestirGpuIn & in, RestirGpuOut * out );
//! interop: the g-buffer (W x H x 16 bytes) and the ratio (W x H x 8 bytes) exported to GL, with two semaphores;
//! remade when W x H changes (every handle is a new NT handle the caller closes)
struct RestirGpuShare
{
	bool ok = false;
	std::string why;
	void * gbufMemory = nullptr, * outMemory = nullptr;
	uint64_t gbufSize = 0, outSize = 0;   // the allocations' sizes (GL imports exactly these)
	void * vkDone = nullptr, * glDone = nullptr;
};
bool restirGpuShare( DdgiGpuSession * s, int W, int H, RestirGpuShare * out );

} // namespace bakert

#endif // RTDDGI_GPU_H
