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
}  // namespace WwVkPresent
