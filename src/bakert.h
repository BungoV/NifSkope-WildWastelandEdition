#ifndef BAKERT_H
#define BAKERT_H

/* Lane RTBAKE1 (2026-10-21): the bakes' shared ray back end. A bake that casts many rays through a probebvh tree
 * can hand them to the GPU in batches. Three tracers, picked at run time:
 *   rt       Vulkan ray query (VK_KHR_ray_query) against a BLAS of the tree's triangles, on RT hardware
 *   compute  a Vulkan compute shader walking the very BVH the CPU walks (GPUs without RT)
 *   cpu      probebvh itself, the reference (and what runs when no Vulkan device is there)
 * WW_BAKE_RT_RED=cpu forces the CPU (the bakes' own code path, byte-identical to before this lane), =compute the
 * non-RT GPU path, =rt the RT path (falls to cpu when absent, and says so). WW_BAKE_THREADS=n sets the worker count
 * of every bake pool (default: all cores).
 *
 * A GPU hit is a candidate only: refine() re-tests the GPU's triangle in double with probebvh's own test (and its
 * alpha hole); a nearest-hit ray then walks the CPU tree bounded at that distance, which returns the CPU's own
 * triangle (coplanar copies and ties included) cheaply. A hit the double test refuses (a graze the float test let
 * through) is re-cast on the CPU in full, and so is a nearest-hit miss: those rays then equal the CPU's exactly. An
 * occlusion (anyHit) miss stays a miss -- the one difference left. */

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace probebvh { struct Bvh; }

namespace bakert {

//! the CPU workers a bake pool starts: WW_BAKE_THREADS when set (>= 1), else every core
inline int threads()
{
	static const int n = []() {
		const char * e = std::getenv( "WW_BAKE_THREADS" );
		const int v = e ? std::atoi( e ) : 0;
		return v > 0 ? v : std::max( 1, int( std::thread::hardware_concurrency() ) );
	}();
	return n;
}

enum class Path { Cpu = 0, Compute = 1, Rt = 2 };
const char * pathName( Path p );

//! a ray in the tree's local frame; a hit counts in (tmin, tmax]
struct Ray { float o[3]; float tmin; float d[3]; float tmax; };
//! tri = the soup triangle (-1: a miss), t its float distance
struct Hit { float t; int tri; };

//! the path a new Scene takes: the red, then what the machine has (RT > compute > cpu); `why` says how it was chosen
Path pickPath( std::string * why = nullptr );
//! the GPU's name ("" without one)
std::string deviceName();

/* One tree on the GPU. Built from a probebvh::Bvh (its triangles, BVH and alpha masks); traces batches of rays in
 * order. Thread-safe (one GPU queue, a lock). Keep the Bvh alive as long as the Scene (refine reads it). */
class Scene
{
public:
	//! the tracer for `bvh` on the picked path; never null (cpu when nothing else works -- path() tells)
	static std::unique_ptr<Scene> make( const probebvh::Bvh & bvh, std::string * log = nullptr );
	~Scene();
	Path path() const { return path_; }
	/* trace n rays (nearest hit, alpha holes pass); the cpu path runs probebvh on the bake pool. anyHit: an occlusion
	 * ray, the GPU stops at its first accepted hit (any triangle, not the nearest; refine still re-tests it) */
	void trace( const Ray * rays, size_t n, Hit * hits, bool anyHit = false );
	/* The answer the CPU bake reads for a GPU hit h of the double ray (o, d) in (1e-4, tmax]: true + t + tri for a hit.
	 * The GPU's triangle re-tested in double; a refused one re-cast on the CPU. exact (nearest-hit rays): the CPU walk
	 * bounded at the re-tested t then gives the CPU's own triangle, ties included, and a miss is re-cast in full; an
	 * occlusion ray passes false. */
	bool refine( const Hit & h, const double o[3], const double d[3], double tmax, double * t, int * tri, bool exact = true ) const;
	// counters for the bake census: rays traced here, rays the refine re-cast on the CPU, GPU milliseconds
	std::atomic<long long> traced { 0 }, recast { 0 };
	std::atomic<long long> gpuUs { 0 };
	struct Impl;
private:
	Scene() = default;
	Path path_ = Path::Cpu;
	const probebvh::Bvh * bvh_ = nullptr;
	Impl * d_ = nullptr;
};

//! a census line: "<who> rays: <path> (<device>), traced n, re-cast on the CPU m, GPU s ms"
std::string census( const char * who, const Scene & s );

} // namespace bakert

#endif // BAKERT_H
