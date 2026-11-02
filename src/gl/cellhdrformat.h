#ifndef CELLHDRFORMAT_H
#define CELLHDRFORMAT_H

#include <cstring>

/* lane FRAMEFMT1 (GFXR-FRAME G2): the cell's linear HDR frame format, as a pure rule (tests/framefmt).
 * Default RGBA16F: half the bytes of RGBA32F on every full-frame touch (the MSAA frame, its tone map, and the
 * copies resolved from it, which follow it: a multisampled resolve needs equal formats). TIMING1 c3 High: f16 vs f32
 * px>16 0.02 %, no visible change. WW_CELL_HDR_FORMAT=rgba32f | rgba16f | r11g11b10f picks one (r11g11b10f has no
 * alpha: a cost arm, never a default). Reds (RGBA32F whatever was asked, the frame as before):
 * WW_FRAMEFMT1_RED=1, WW_CELL_HDR_FORMAT_RED=1, WW_AAFORMAT1_RED=1. */
struct WwHdrFormatRule
{
	unsigned gl = 0x881A;	// GL_RGBA16F
	int bytes = 8;
	const char * name = "rgba16f";
	bool red = false;
};

//! `v`: WW_CELL_HDR_FORMAT, trimmed and lower case (may be null); `red`: any of the reds set.
inline WwHdrFormatRule wwHdrFormatOf( const char * v, bool red )
{
	WwHdrFormatRule h;
	const auto f32 = [&h]() { h.gl = 0x8814; h.bytes = 16; h.name = "rgba32f"; };	// GL_RGBA32F
	if ( red ) {
		f32();
		h.red = true;
		return h;
	}
	if ( v && std::strcmp( v, "rgba32f" ) == 0 ) {
		f32();
	} else if ( v && std::strcmp( v, "r11g11b10f" ) == 0 ) {
		h.gl = 0x8C3A; h.bytes = 4; h.name = "r11g11b10f";	// GL_R11F_G11F_B10F
	}
	return h;
}

/* lane PRESETPERF1 (FRAMEFMT1-B): the two shader-written copies of the frame that FRAMEFMT1 left at RGBA32F, the
 * exposure measure's linear copy (cellhdr.cpp IsMeasureGpu::tex, full size + mips to the quarter) and the GPU bloom's
 * quarter finals (btex). Neither is resolved from the multisampled frame, so either format is legal. Default RGBA32F
 * (today); WW_CELL_POST_FORMAT=rgba16f halves their bytes (a cost arm until a hold proves "same picture, less time").
 * Red WW_FRAMEFMT1B_RED=1: RGBA32F whatever was asked. */
inline WwHdrFormatRule wwPostFormatOf( const char * v, bool red )
{
	WwHdrFormatRule h;
	h.gl = 0x8814; h.bytes = 16; h.name = "rgba32f";	// GL_RGBA32F
	if ( red ) {
		h.red = true;
		return h;
	}
	if ( v && std::strcmp( v, "rgba16f" ) == 0 ) {
		h.gl = 0x881A; h.bytes = 8; h.name = "rgba16f";	// GL_RGBA16F
	}
	return h;
}

#endif
