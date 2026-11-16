#pragma once

// Lane VKAIR17 (2026-11-16): the air's lamp depth atlas (airlampatlas.cpp, D16 2D, read with a compare sampler by
// cell_volair's stages 1 and 4) made as a shared target of the present device while the air view is on Vulkan, so the
// Vulkan readers (vkairtemporal.inc, vkairlamp.inc) sample it in place (farSharedOf) instead of refusing it by name.
// Option A (share the GL atlas): the atlas is 8192^2 D16 (128 MB), never copied a run; its tiles stay drawn by GL through
// the FBO made here. Option B (draw the tiles on Vulkan) would port every caster's cube-face depth draw: not now.
// The table (volLampTable, 4 RGBA32F texels a lamp) stays GL's: the readers copy it a run like the light list.
// A make (first use, a wish flip, a size change, a present shutdown that dropped the name) returns 1: the caller draws
// every tile again (its tile memo is void). A refused share is latched until the wish turns off (no retry a frame).
// Gate: tests/vkframe/airlampview.sh and airtemporalview.sh, the atlas arm (this helper at the app's size).

#include <cstdint>
#include <string>

#include "features/vkframe/vkpresent.h"

//! the atlas's ownership; lives outside the atlas's own state (that one is reset on release)
struct AirAtlasShare
{
	bool shared = false;   //!< the name is the present device's
	bool refused = false;  //!< the wish was refused once (latched until the wish turns off)
	int size = 0;          //!< the size the name was made for (0: none)
	std::string why;       //!< the last refusal's reason
};

//! gate-only reds (the app never sets them): 1 the shared make is skipped (GL's own atlas while the share is wished)
inline int & airAtlasRed()
{
	static int r = 0;
	return r;
}

//! drop the atlas and its framebuffer (a shared name goes back to the present device, never glDeleteTextures)
template <class Fn>
void airAtlasRelease( Fn * fn, std::uintptr_t key, unsigned & tex, unsigned & fbo, AirAtlasShare & sh )
{
	if ( fbo )
		fn->glDeleteFramebuffers( 1, &fbo );
	fbo = 0;
	if ( tex ) {
		if ( sh.shared ) {
			if ( WwVkPresent::sharedTargetIs( key, tex ) )
				WwVkPresent::sharedTargetDrop( key );
		} else {
			fn->glDeleteTextures( 1, &tex );
		}
	}
	tex = 0;
	sh.shared = false;
	sh.size = 0;
}

/* tex holds a size^2 D16 compare atlas, shared while want (and not refused), with fbo its depth framebuffer (fboOk: complete).
 * Returns 0 when it already did (a lookup only), 1 when it was made (empty: draw every tile). Leaves GL_TEXTURE_2D of the
 * active unit at 0 and the draw framebuffer as it found it. */
template <class Fn>
int airAtlasEnsure( Fn * fn, std::uintptr_t key, int size, bool want, unsigned & tex, unsigned & fbo, bool & fboOk, AirAtlasShare & sh )
{
	if ( !want )
		sh.refused = false;
	const bool wantS = want && !sh.refused;
	if ( tex && sh.size == size && sh.shared == wantS && ( !sh.shared || WwVkPresent::sharedTargetIs( key, tex ) ) )
		return 0;
	airAtlasRelease( fn, key, tex, fbo, sh );
	const unsigned kTex2d = 0x0DE1, kD16 = 0x81A5;
	if ( wantS && !( airAtlasRed() & 1 ) ) {
		unsigned t = 0;
		std::string why;
		if ( WwVkPresent::sharedTarget( key, 0, size, size, 1, kD16, t, why ) && t ) {
			tex = t;
			sh.shared = true;
			sh.why.clear();
		} else {
			sh.refused = true;
			sh.why = why;
		}
	}
	if ( !sh.shared ) {
		fn->glGenTextures( 1, &tex );
		fn->glBindTexture( kTex2d, tex );
		fn->glTexImage2D( kTex2d, 0, int( kD16 ), size, size, 0, 0x1902 /* GL_DEPTH_COMPONENT */, 0x1403 /* GL_UNSIGNED_SHORT */, nullptr );
	} else {
		fn->glBindTexture( kTex2d, tex );
	}
	fn->glTexParameteri( kTex2d, 0x2801, 0x2601 );  // GL_TEXTURE_MIN_FILTER GL_LINEAR
	fn->glTexParameteri( kTex2d, 0x2800, 0x2601 );  // GL_TEXTURE_MAG_FILTER GL_LINEAR
	fn->glTexParameteri( kTex2d, 0x2802, 0x812F );  // GL_TEXTURE_WRAP_S GL_CLAMP_TO_EDGE
	fn->glTexParameteri( kTex2d, 0x2803, 0x812F );  // GL_TEXTURE_WRAP_T GL_CLAMP_TO_EDGE
	fn->glTexParameteri( kTex2d, 0x813D, 0 );       // GL_TEXTURE_MAX_LEVEL
	fn->glTexParameteri( kTex2d, 0x884C, 0x884E );  // GL_TEXTURE_COMPARE_MODE GL_COMPARE_REF_TO_TEXTURE
	fn->glTexParameteri( kTex2d, 0x884D, 0x0203 );  // GL_TEXTURE_COMPARE_FUNC GL_LEQUAL
	fn->glBindTexture( kTex2d, 0 );
	int prevDraw = 0;
	fn->glGetIntegerv( 0x8CA6 /* GL_DRAW_FRAMEBUFFER_BINDING */, &prevDraw );
	fn->glGenFramebuffers( 1, &fbo );
	fn->glBindFramebuffer( 0x8CA9 /* GL_DRAW_FRAMEBUFFER */, fbo );
	fn->glFramebufferTexture2D( 0x8CA9, 0x8D00 /* GL_DEPTH_ATTACHMENT */, kTex2d, tex, 0 );
	fn->glDrawBuffer( 0 );
	fboOk = fn->glCheckFramebufferStatus( 0x8CA9 ) == 0x8CD5;  // GL_FRAMEBUFFER_COMPLETE
	fn->glBindFramebuffer( 0x8CA9, unsigned( prevDraw ) );
	sh.size = size;
	return 1;
}

//! the `vk airatlas:` line (said by the owner once a make)
inline std::string airAtlasSay( int size, bool want, bool fboOk, const AirAtlasShare & sh )
{
	std::string s = "vk airatlas: " + std::to_string( size ) + " px D16 ";
	if ( sh.shared )
		s += "shared (the Vulkan air readers sample it in place)";
	else if ( want )
		s += "on GL (share refused: " + sh.why + ")";
	else
		s += "on GL (the air view not on Vulkan)";
	if ( !fboOk )
		s += " fbo INCOMPLETE";
	return s;
}
