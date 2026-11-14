#pragma once

/* Lane VKOBJ3 (2026-11-13, plan row G4 slice 3): the cell's own textures in the bindless table. With WW_VKOBJ_TEX=1 the
 * texture cache makes a DDS's block-compressed 2D texture on the present device (VKFAR1's farTwin: the Vulkan image, GL
 * imports it and uploads into it as into its own texture), and WwBl's resident handles of such a texture join the table
 * (blAdd with the GL sampler state the handle froze). A texture the cache deletes leaves the table at once and its Vulkan
 * half at the next frame (one idle wait for the batch). The core has no Qt: the switch and the red are read by the callers.
 * Gate: tests/vkframe/blview.sh bc (BC1..BC7 twins, GL's handles against the table, the red `key` must FAIL). */

#include "features/vkframe/vkpresent.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace WwVkPresent
{
/*! a 2D texture of glFmt (a BC name: S3TC / RGTC / BPTC, sRGB too) made on the present device; tex = the GL name GL
 * uploads into (immutable storage of `levels`). false with a reason (no device in this context, not a BC format...) */
bool objTexMake( unsigned glFmt, int w, int h, int levels, unsigned & tex, std::string & why );
//! tex is a twin objTexMake made (and not yet gone)
bool objTexIs( unsigned tex );
/*! GL made `handle` resident for tex: a twin's handle joins the table with tex's sampler state (read once here). red 1
 * (WW_VKOBJ_TEX_RED=key): the handle is added with the twin made before tex's (the wrong image); red 2
 * (WW_VKOBJ_TEX_RED=swz): with no swizzle (BC4's R,R,R,1 view dropped) */
void objTexHandle( unsigned tex, std::uint64_t handle, GlProc gp, int red = 0 );
//! the cache deletes tex next: a twin's handles leave the table, its Vulkan half goes at objTexFrame (GL's name is the cache's)
void objTexGone( unsigned tex );
//! the rows of a frame GL drew with handles (n x 176 bytes): translated as the Vulkan draw would, counted for the line
void objTexRows( const void * glRows, std::size_t n );

struct ObjTexStat
{
	int twins = 0, made = 0, refused = 0, gone = 0, freed = 0;
	int handles = 0, adds = 0, addFails = 0;
	std::size_t rows = 0, whole = 0;  //!< rows translated, those whose every handle the table knew
	int images = 0, samplers = 0, misses = 0;  //!< the table's live images / samplers, unknown handles in the rows
	double mb = 0.0, makeMs = 0.0, freeMs = 0.0;
	std::string why, addWhy;  //!< the last refusal (make / add)
	//! VKOBJ4: twins made for the batch's hand-over, the flushes (each one GL signal + one submit), their twins and time
	int batched = 0, flushes = 0, flushed = 0, flushFails = 0;
	int swizzled = 0;  //!< adds through a swizzle's own view (BC4's R,R,R,1)
	double flushMs = 0.0;
	//! VKOBJ4: the cell's load so far (made + flush time; it ends 30 frames after the last make)
	int loadTwins = 0, loadFlushes = 0;
	double loadMs = 0.0;
};
/*! VKOBJ4: the twins made since the last flush go to the present device as GENERAL in one hand-over (GL signals once with
 * all of them, one submit waits it). objTexFrame calls it; a Vulkan draw that samples them calls it first */
void objTexFlush();
/*! once a frame: the batch flushed, the twins gone since the last call freed (drain + one idle wait), then the counters
 * since the last call (twins / handles / mb: live; load: the cell's) */
ObjTexStat objTexFrame();
//! shutdown(): the twins the cache still holds keep their GL names (on GL's imported memory); returns how many
int objTexFree();
}  // namespace WwVkPresent
