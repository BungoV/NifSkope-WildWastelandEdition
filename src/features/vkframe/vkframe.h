#ifndef WW_VKFRAME_H
#define WW_VKFRAME_H

/* Lane VKFRAME1 round 2 (2026-11-12): the live glue of the Vulkan frame (notes/vkframe1/plan.md). The device is bakert's
 * (src/features/gi/bakert.cpp registers it with wwVkFrameSetDevice once it is made with GL sharing); this file owns
 * the passes that moved: images Vulkan makes and GL imports (GL_EXT_memory_object), one binary semaphore each way
 * (GL_EXT_semaphore), a fence ring for the frames in flight, the pipelines built at registration behind an on-disk
 * pipeline cache, the GPU timestamps (PassTime sub vk-bloom, `hardware use:`) and the `vk frame:` line.
 * The rules it follows are vkframegraph.h's (tests/vkframe/framegraph.sh). GL thread only, except wwVkFrameSetDevice. */

#include <QString>

#include <mutex>

//! bakert's device, once made with GL sharing (its external memory + semaphore extensions on). Builds every pipeline.
void wwVkFrameSetDevice( void * instance, void * physical, void * device, unsigned family, void * queue,
	void * getDeviceProcAddr, const unsigned char deviceUuid[16], std::mutex * queueLock );

namespace WwVkPost
{

constexpr int kMaxMips = 12;
constexpr int kRingLayers = 8;

/* The bloom chain on Vulkan (step P1 of the plan: down / hold / up; cellbloom.cpp keeps the source, the dump, the
 * combine and the top's mips on GL, on the same texture names). Off: WW_VKPOST_RED=1, or Settings > Ray tracing
 * device > Vulkan Post unticked. Read every frame (the row is read again each second). */
bool wanted();
//! wanted() and the device registered with its pipelines built: worth making the chain's images on Vulkan
bool ready();

//! the chain's textures as Vulkan images GL imported; false = none made (the caller makes its own GL ones; why says why)
bool makeChain( const void * key, int W, int H, int mipCount, const int dims[][2], unsigned & src, unsigned mip[], unsigned up[],
	unsigned & ring, QString & why );

/* round 3 (P3): the linear frame's multisample colour (RGBA16F) and depth / stencil (dsFormat: GL_DEPTH24_STENCIL8 or
 * GL_DEPTH32F_STENCIL8) as images Vulkan made and GL imported (cellhdr.cpp draws the frame into them), so the bloom's
 * source reads them on Vulkan. false = none made (why): the caller makes its own GL ones. GL names, not deleted by the caller. */
bool makeFrame( const void * key, int W, int H, int samples, unsigned dsFormat, unsigned & color, unsigned & ds, QString & why );
void freeFrame( const void * key );
//! the frame (these GL names) is shared and the chain runs on Vulkan: runBloom can make the source (BloomJob::src)
bool sourceReady( const void * key, unsigned color, unsigned ds );
//! the combine's RGBA32F target (GL name) if the last runBloom ran the combine (BloomJob::comb), else 0
unsigned combTexture( const void * key );
//! drops the images (and their GL textures: the caller does not delete them)
void freeChain( const void * key );
bool hasChain( const void * key );

struct BloomJob
{
	float thr = 1.0f, knee = 0.5f;
	int red = 0;	// 1: level 0 a plain box (WW_CELL_BLOOM_RED=nokaris)
	bool hold = false;
	int holdK = 0, holdN = 1;
	float radius = 1.0f, dispersion = 0.0f;
	float srcW[kMaxMips] = {}, accW[kMaxMips] = {};
	/* round 3 (P3): the source on Vulkan from the shared frame (sourceReady), cell_bloom_src.frag's uniforms: exposure
	 * (1 for the red's noexp), on (cellOn && cellIsOn), the test impulse (value > 0 at x, y: no stencil) */
	bool src = false;
	unsigned frameColor = 0, frameDs = 0;
	float exposure = 1.0f;
	bool on = false;
	float impulseX = 0.0f, impulseY = 0.0f, impulse = 0.0f;
	// round 3 (P2): the final tent once per pixel into an RGBA32F target (cell_bloom_comb.frag at lod 0)
	bool comb = false;
};

/* the down, hold and up dispatches between GL's source resolve and GL's reads of the top (round 3: with job.src the source
 * before them, with job.comb the combine after). false: nothing ran (why), the caller runs its GL passes on the same
 * textures. Called on every frame the chain runs, so the route's changes are logged. */
bool runBloom( const void * key, const BloomJob & job, QString & why );

}	// namespace WwVkPost

//! `hardware use:` words (its 5 s window); also prints the `vk frame:` line with the CPU / GPU overlap of the window
QString wwVkFrameUse( int frames, double glGpuMs, double wallMs );

#endif
