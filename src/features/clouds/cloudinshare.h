#ifndef CLOUDINSHARE_H
#define CLOUDINSHARE_H

/* lane VKCLOUD10: the clouds' density inputs as shared targets while the view is on Vulkan. cvShape and cvDetail
 * (cloudvol.cpp) and cvMap (cloudmap.cpp) then live on the present device, so the march (cloudRun) and the fly bake
 * (flyBakeRun) read them in place through farSharedOf instead of copying them. GL draws and samples the same names.
 * With want false (the view on GL, red WW_VKVIEW_CLOUDIN_RED=1, a refusal) the calls are the old ones, byte for byte.
 * No Qt, no GL header (GL enums as numbers): the gates (tests/vkframe/fxview.cpp, flybakeview.cpp) run these same
 * functions. Fn: any table with the GL entry points named as GL names them (Qt's GLFunctions, a gate's struct).
 * Never detect a remake by the GL name: NVIDIA hands the same name back after a drop. */

#include "features/vkframe/vkpresent.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace WwCloudInShare
{
//! red bits (gates only): 1 a remake keeps its old state (noise: no fill; map: not attached to the FBO), 2 never shared
inline constexpr unsigned kRedStale = 1, kRedUnshared = 2;

struct Vol
{
	unsigned tex = 0;
	bool shared = false, refused = false;
	unsigned long long made = 0;  //!< (re)makes: new contents
};

//! the levels of a full chain down to 1 (128 -> 8): what upload's glGenerateMipmap makes and fxTexOf counts
inline int chainLevels( int n )
{
	int l = 1;
	while ( ( n >> l ) > 0 )
		l++;
	return l;
}

/* a noise volume (n^3 R8, trilinear repeat, every level) at tex. Kept while its share state holds and the shared one
 * is live; else (re)made and filled from bytes. Shared: sharedTarget with the whole chain, level 0 by glTexSubImage3D,
 * the rest by glGenerateMipmap (glTexImage is invalid on a shared name). false: no texture. */
template <class Fn> bool ensureVol( Fn * fn, Vol & v, std::uintptr_t key, const std::uint8_t * bytes, int n, bool want,
	const char * name, unsigned red = 0 )
{
	if ( !want )
		v.refused = false;  // asked again later: one more try
	want = want && !v.refused && !( red & kRedUnshared );
	const bool live = v.shared && WwVkPresent::sharedTargetIs( key, v.tex );
	if ( v.shared && !live )
		v.tex = 0;  // a shutdown dropped it (its GL name with it)
	if ( v.tex && v.shared == want )
		return true;
	const bool was = v.shared;
	if ( v.shared ) {
		WwVkPresent::sharedTargetDrop( key );
		v.tex = 0;
		v.shared = false;
	}
	if ( want ) {
		unsigned t = 0;
		std::string why;
		if ( WwVkPresent::sharedTarget( key, 1, n, n, n, 0x8229u /* GL_R8 */, t, why, chainLevels( n ) ) ) {
			if ( v.tex )
				fn->glDeleteTextures( 1, &v.tex );
			v.tex = t;
			v.shared = true;
		} else {
			v.refused = true;
			std::fprintf( stderr, "cloudvol: %s stays GL's own (shared target refused: %s)\n", name, why.c_str() );
			if ( v.tex )
				return true;  // the plain one holds its contents
		}
	}
	v.made++;
	if ( !v.shared ) {  // the old upload3d
		fn->glGenTextures( 1, &v.tex );
		fn->glBindTexture( 0x806F /* GL_TEXTURE_3D */, v.tex );
		fn->glPixelStorei( 0x0CF5 /* GL_UNPACK_ALIGNMENT */, 1 );
		fn->glTexImage3D( 0x806F, 0, 0x8229 /* GL_R8 */, n, n, n, 0, 0x1903 /* GL_RED */, 0x1401 /* GL_UNSIGNED_BYTE */, bytes );
		fn->glPixelStorei( 0x0CF5, 4 );
		fn->glGenerateMipmap( 0x806F );
	} else {
		fn->glBindTexture( 0x806F, v.tex );
		if ( !( red & kRedStale ) ) {
			fn->glPixelStorei( 0x0CF5, 1 );
			fn->glTexSubImage3D( 0x806F, 0, 0, 0, 0, n, n, n, 0x1903, 0x1401, bytes );
			fn->glPixelStorei( 0x0CF5, 4 );
			fn->glGenerateMipmap( 0x806F );
		}
	}
	fn->glTexParameteri( 0x806F, 0x2801 /* MIN_FILTER */, 0x2703 /* LINEAR_MIPMAP_LINEAR */ );
	fn->glTexParameteri( 0x806F, 0x2800 /* MAG_FILTER */, 0x2601 /* LINEAR */ );
	for ( unsigned w : { 0x2802u, 0x2803u, 0x8072u } )  // WRAP_S / T / R
		fn->glTexParameteri( 0x806F, w, 0x2901 /* REPEAT */ );
	fn->glBindTexture( 0x806F, 0 );
	if ( v.shared != was )
		std::fprintf( stderr, "cloudvol: %s %d^3 noise %s\n", name, n, v.shared ? "on the present device (shared with GL)" : "on GL's own texture" );
	return v.tex != 0;
}

/* the map's target (n x n of glFmt, linear clamp, one level) attached to fbo's colour 0. Kept while its size and share
 * state hold and the shared one is live; else (re)made: remade = true, the contents are gone and the caller draws the
 * map again. The plain path is cloudmap.cpp's old ensureTarget texture calls. The caller sets the draw buffers and
 * checks the FBO. */
template <class Fn> bool ensureMap( Fn * fn, Vol & v, int & size, unsigned fbo, std::uintptr_t key, int n, unsigned glFmt, bool want,
	bool & remade, unsigned red = 0 )
{
	remade = false;
	if ( !want )
		v.refused = false;
	want = want && !v.refused && !( red & kRedUnshared );
	const bool live = v.shared && WwVkPresent::sharedTargetIs( key, v.tex );
	if ( v.shared && !live )
		v.tex = 0;
	if ( v.tex && size == n && v.shared == want )
		return true;
	const bool was = v.shared;
	if ( v.shared ) {
		WwVkPresent::sharedTargetDrop( key );
		v.tex = 0;
		v.shared = false;
	}
	if ( want ) {
		unsigned t = 0;
		std::string why;
		if ( WwVkPresent::sharedTarget( key, 0, n, n, 1, glFmt, t, why, 1 ) ) {
			if ( v.tex )
				fn->glDeleteTextures( 1, &v.tex );
			v.tex = t;
			v.shared = true;
		} else {
			v.refused = true;
			std::fprintf( stderr, "cloudmap: %dx%d map stays GL's own (shared target refused: %s)\n", n, n, why.c_str() );
		}
	}
	v.made++;
	remade = true;
	if ( !v.shared ) {
		if ( v.tex )
			fn->glDeleteTextures( 1, &v.tex );
		fn->glGenTextures( 1, &v.tex );
		fn->glBindTexture( 0x0DE1 /* GL_TEXTURE_2D */, v.tex );
		fn->glTexImage2D( 0x0DE1, 0, int( glFmt ), n, n, 0, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, nullptr );
	} else {
		fn->glBindTexture( 0x0DE1, v.tex );
	}
	fn->glTexParameteri( 0x0DE1, 0x2801, 0x2601 /* LINEAR */ );
	fn->glTexParameteri( 0x0DE1, 0x2800, 0x2601 );
	fn->glTexParameteri( 0x0DE1, 0x2802, 0x812F /* CLAMP_TO_EDGE */ );
	fn->glTexParameteri( 0x0DE1, 0x2803, 0x812F );
	fn->glBindTexture( 0x0DE1, 0 );
	fn->glBindFramebuffer( 0x8D40 /* GL_FRAMEBUFFER */, fbo );
	if ( !( v.shared && ( red & kRedStale ) ) )
		fn->glFramebufferTexture2D( 0x8D40, 0x8CE0 /* COLOR_ATTACHMENT0 */, 0x0DE1, v.tex, 0 );
	size = n;
	if ( v.shared != was )
		std::fprintf( stderr, "cloudmap: %dx%d map %s\n", n, n, v.shared ? "on the present device (shared with GL)" : "on GL's own texture" );
	return v.tex != 0;
}

//! the "vk clouds:" / "vk cloudflybake:" lines' names of FxStat::inPlace ("none" when every input was copied)
inline std::string inPlaceNames( unsigned m )
{
	std::string s;
	const char * const n[4] = { "cvShape", "cvDetail", "cvMap", "cfSdf" };
	for ( int i = 0; i < 4; i++ )
		if ( m & ( 1u << i ) )
			s += s.empty() ? n[i] : std::string( " " ) + n[i];
	return s.empty() ? "none" : s;
}

//! the map target is stale (its share state moved, or the shared one went): the caller rebuilds the map this frame
inline bool mapStale( const Vol & v, std::uintptr_t key, bool want, unsigned red = 0 )
{
	want = want && !v.refused && !( red & kRedUnshared );
	return v.tex && ( v.shared != want || ( v.shared && !WwVkPresent::sharedTargetIs( key, v.tex ) ) );
}
}  // namespace WwCloudInShare

#endif
