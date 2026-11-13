#pragma once

/* Lane VKOBJ2 (2026-11-13, plan row G4 slice 2, the bindless half): GL's bindless texture handles as Vulkan descriptor
 * indexing on the present device. GL's draw row (src/features/near/cellbindless.h WwBl::Row, 176 bytes) holds a 64-bit
 * resident handle per map; the Vulkan row holds (image index, sampler index) into two runtime-sized arrays of one
 * descriptor set (binding 0 the sampled images, binding 1 the samplers; partially bound, updated after bind and while
 * unused by pending work). DXC 1.9 cannot declare a runtime array of combined image samplers, so a map is the pair the
 * descriptor-heap twin (Fusion Shaders) reads too: res/shaders/hlsl/near/ww_bindless_vk.hlsli.
 * The table: blAdd( handle, key, sampler ) when GL makes a handle resident (key: the owner's shared twin, sharedTarget or,
 * once VKFAR1 lands, its BC twin), blDrop( handle ) when it goes non-resident, blTranslate( rows ) per frame. Index 0 of
 * both arrays is a 1x1 black image and GL's default sampler: handle 0 and unknown handles read it (counted as misses).
 * A dropped image index is reused only after 8 translates (the present ring). Gate: tests/vkframe/blview.sh. */

#include "features/vkframe/vkpresent.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace WwVkPresent
{

//! the GL sampler state a bindless handle froze (GL enums as GL reports them)
struct BlSampler
{
	unsigned minFilter = 0x2702, magFilter = 0x2601;  // GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR (GL's defaults)
	unsigned wrapS = 0x2901, wrapT = 0x2901;          // GL_REPEAT
	float lodBias = 0.0f, minLod = -1000.0f, maxLod = 1000.0f, aniso = 1.0f;
	int baseLevel = 0, maxLevel = 1000, levels = 1;   // levels: the texture's immutable level count
	unsigned compareMode = 0, srgbDecode = 0x8A49;    // GL_NONE, GL_DECODE_EXT
	int swizzle[4] = { 0x1903, 0x1904, 0x1905, 0x1906 };  // GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA
	float border[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

//! the GL texture's sampler state (glGetTextureParameter*, GL 4.5); false with a reason when an entry point is missing
bool blSamplerRead( unsigned tex, BlSampler & s, GlProc gp, std::string & why );

struct BlStat
{
	int images = 0, samplers = 0;  //!< live image indices (without the default), distinct samplers
	int adds = 0, drops = 0, rows = 0, misses = 0, rewrites = 0;
	int fails = 0;
	std::string failWhy;
	std::uint32_t imageCap = 0, samplerCap = 0;
};

//! empty when the present device can hold the table, else why not (no descriptor indexing...)
std::string blRefusal();

/* handle (GL's resident 64-bit handle) -> the image of the shared twin `key` (2D, one sample) and the sampler of state s.
 * false with a reason (no twin, a sampler GL state Vulkan cannot match, the arrays full): the draw stays on GL */
bool blAdd( std::uint64_t handle, std::uintptr_t key, const BlSampler & s, std::string & why );

//! the handle went non-resident: its image index goes to the graves when no other handle names it
void blDrop( std::uint64_t handle );

/* GL rows (n x 176 bytes) into Vulkan rows: everything copied, tex[i] = (image index, sampler index). Returns the rows
 * whose every non-zero handle was known. red (gate only): 1 = image index + 1 (the table shifted), 2 = every sampler
 * GL's linear repeat (the sampler state ignored) */
std::size_t blTranslate( const void * glRows, void * vkRows, std::size_t n, int red = 0 );

BlStat takeBlStat();

}  // namespace WwVkPresent
