#pragma once
/* lane VKBUF1 (2026-11-16): the RT GI's texel buffers on the present device, found by name. cellDdgiAtlas (the cascades'
 * buffer, GL copies every cascade's atlas into it) and cellRestir (ReSTIR DI's ratio, RG32F; GL copies bakert's output into
 * it once a step) are farTexelBuffers keyed by a static char per slot (vkrtbuf.inc), so a pass on the view binds the view
 * and reads GL's bytes in place. GL writes them before its crossing, so a reader behind the crossing sees them.
 * Red WW_VKVIEW_RTBUF_RED=1: nothing shared, every lookup refuses (the consumer then refuses by name). */
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace WwVkPresent
{
enum RtBufSlot
{
	kRtBufDdgiAtlas = 0,  //!< "cellDdgiAtlas", RGBA32F (probes x 193 texels a cascade)
	kRtBufRestir = 1,     //!< "cellRestir", RG32F (W x H texels: the ratio, the traced depth)
	kRtBufSlots = 2
};
const char * rtBufName( int slot );  //!< the HLSL / GL uniform name; nullptr past the slots
int rtBufSlotOf( const char * name );  //!< -1 when no slot carries that name
std::uintptr_t rtBufKey( int slot );  //!< the farTexelBuffer key (a static char's address, never inside a moving value)
bool rtBufRed();  //!< WW_VKVIEW_RTBUF_RED=1
/*! owner: the slot's buffer made (or kept) at `bytes` of glFmt (GL_RGBA32F, GL_RG32F). glBuf / glTex are GL's names, its
 * GL_TEXTURE_BUFFER view already attached; a new size gives new names (the owner re-attaches). Write it with
 * glBufferSubData / copies, never glBufferData. false: why (the red, no device, a format or size the device refuses) */
bool rtBufShare( int slot, unsigned glFmt, std::size_t bytes, unsigned & glBuf, unsigned & glTex, std::string & why );
//! owner: GL's buffer srcGlBuf's first `bytes` copied into the slot's buffer (glCopyNamedBufferSubData: GL work)
bool rtBufCopy( int slot, unsigned srcGlBuf, std::size_t bytes, std::string & why );
//! owner: the slot's buffer and GL names gone (the worker drained, the device idle); a lookup refuses until it is made again
void rtBufDrop( int slot );
/*! owner: called by a lookup (GL's thread, GL current) before it answers: brings the slot up to the owner's newest data
 * (ReSTIR: its wait on the step, then the copy once a step). false + why: nothing to read this frame. nullptr: none */
void rtBufOwner( int slot, std::function<bool( std::string & why )> fresh );
struct RtBuf
{
	std::uint64_t view = 0;    //!< VkBufferView as a handle
	std::uint64_t texels = 0;  //!< the view's texels
	std::uint64_t bytes = 0;
	std::uintptr_t key = 0;    //!< farTexelBufferRead's key (gates)
	unsigned glBuf = 0;        //!< GL's name of the buffer (the crossing's buffer list)
	unsigned glFmt = 0;
	std::uint64_t gen = 0;     //!< rtBufGen() at the answer
};
/*! consumer: the shared buffer by name ("cellDdgiAtlas", "cellRestir"). false + why when it is not on the present device
 * (the red, no owner data, never made): the consumer refuses that input by name. A descriptor set holding the view is
 * written again when rtBufGen() moved (never by comparing handles: the driver reuses them). */
bool rtBufOf( const char * name, RtBuf & out, std::string & why );
std::uint64_t rtBufGen();  //!< moves whenever a slot's buffer is made or dropped
struct RtBufStat
{
	int made[kRtBufSlots] = {}, dropped[kRtBufSlots] = {}, refused[kRtBufSlots] = {};
	std::uint64_t lookups[kRtBufSlots] = {}, copies[kRtBufSlots] = {}, copyBytes[kRtBufSlots] = {};
	std::uint64_t bytes[kRtBufSlots] = {};  //!< the live buffer's size (0: none)
	std::string why[kRtBufSlots];           //!< the last refusal
};
RtBufStat rtBufStat();
//! the `vk rtbuf:` line (both slots: size, made / dropped, lookups, copies, the last refusal, the red)
std::string rtBufLine();
//! the line on stderr, at most once a 5 s window and only when it changed
void rtBufSay();

/* lane VKBUF2 (2026-11-16): the screen GI's images on the present device, found by name. cellGiHalf (the GI sample) and
 * cellGiHalfKey (its key: world position, packed normal) are RGBA32F 2D sharedTargets, w x h = the view over giStep
 * (rtImgSize), NEAREST, clamp, one level. GL's cell_gihalf pass draws both through one FBO (attachments 0 and 1) before
 * its crossing. Keys are the OWNER's (heap bytes it holds: each view's AoGpu has its own); the slot names the owner that
 * shared last. Red WW_VKVIEW_GIHALF_RED=1 (VKWATER3's): nothing shared, every lookup refuses. */
enum RtImgSlot
{
	kRtImgGiHalf = 0,     //!< "cellGiHalf", RGBA32F (sampler t104 in fo4_cellcsm)
	kRtImgGiHalfKey = 1,  //!< "cellGiHalfKey", RGBA32F (t105)
	kRtImgSlots = 2
};
const char * rtImgName( int slot );    //!< the HLSL / GL uniform name; nullptr past the slots
int rtImgSlotOf( const char * name );  //!< -1 when no slot carries that name
bool rtImgRed();                       //!< WW_VKVIEW_GIHALF_RED=1
//! the screen GI's size along one axis: the view's `full` pixels at one sample per `step` (2 half, 1 full), rounded up
inline int rtImgSize( int full, int step )
{
	return step > 0 ? ( full + step - 1 ) / step : 0;
}
/*! owner: the slot's image made (or kept) as an RGBA32F sharedTarget of w x h under `key` (the owner's heap key, never an
 * address inside a moving value), NEAREST, clamp, max level 0. With fbo != 0 it is attached to that framebuffer's colour
 * attachment `attach` (GL_FRAMEBUFFER binding left on fbo). tex = its GL name; a new size or a remake gives a new one, the
 * GL name the owner had before is the owner's to delete. The slot now names this key. false + why: the red, no device */
bool rtImgShare( int slot, std::uintptr_t key, int w, int h, unsigned fbo, int attach, unsigned & tex, std::string & why );
//! owner: the target of `key` dropped on the present device (its GL name goes too); the slot forgets it when it named that key
void rtImgDrop( int slot, std::uintptr_t key );
/*! owner: called by a lookup (GL's thread, GL current): false + why when GL's program does not read the slot's image this
 * frame (no GI this frame, a probe picture binds the volumes' mask on cellGiHalf, a debug view). nullptr: none */
void rtImgOwner( int slot, std::function<bool( std::string & why )> fresh );
struct RtImg
{
	std::uintptr_t key = 0;  //!< the sharedTarget key (sharedFind / farSharedOf( glTex ) give the same)
	unsigned glTex = 0;      //!< GL's name (the crossing's texture list; GL wrote it)
	unsigned glFmt = 0;      //!< GL_RGBA32F
	std::uint64_t view = 0;  //!< the VkImageView (GENERAL layout), as the target's own view
	int w = 0, h = 0;
	std::uint64_t gen = 0;   //!< rtImgGen() at the answer
};
/*! consumer: the shared image by name ("cellGiHalf", "cellGiHalfKey"). false + why when it is not on the present device
 * (the red, no owner, nothing read this frame, dropped, another size than the owner's): the consumer refuses that input by
 * name. Rewrite descriptor sets when rtImgGen() moved. */
bool rtImgOf( const char * name, RtImg & out, std::string & why );
std::uint64_t rtImgGen();  //!< moves whenever a slot's image is made, remade or dropped
struct RtImgStat
{
	int made[kRtImgSlots] = {}, dropped[kRtImgSlots] = {}, refused[kRtImgSlots] = {};
	std::uint64_t lookups[kRtImgSlots] = {};
	int w[kRtImgSlots] = {}, h[kRtImgSlots] = {};  //!< the live image's size (0: none)
	std::string why[kRtImgSlots];                  //!< the last refusal
};
RtImgStat rtImgStat();
//! the `vk rtimg:` line (both slots: size, made / dropped, lookups, refusals, the last refusal, the red)
std::string rtImgLine();
//! the line on the app's log (stderr without Qt), at most once a 5 s window and only when it changed.
//! The owner calls it once a pass AFTER sharing both slots (rtImgShare / rtImgDrop do not say); lookups say too
void rtImgSay();
}  // namespace WwVkPresent
