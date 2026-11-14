#pragma once

/* Lane VKFAR2 (2026-11-14, plan row G2): the sky bodies on Vulkan, in the sky's own pass. The backdrop arms the bodies
 * (skyBodiesArm) before the physical sky; skyRun then does its copies and uniforms but holds its crossing back. The stars
 * (lookdev_clouds: the CPU-turned mesh, its per-vertex extinction colours) and the moon globe (lookdev_bill + sky_moon:
 * moon_d / moon_n and three of the sky's LUTs) join it as draws after the sky's triangle, each with its program's
 * uniforms read back from GL (fxPack) and its maps' names; skyBodiesFlush sends the one crossing. A body that cannot
 * join flushes first and draws on GL after it, so the order is GL's. The maps are the texture cache's block-compressed
 * twins (vkobjtex.h): a Textures\Sky load makes one while this row is wanted, uploaded once by GL into the Vulkan image.
 * Shaders: res/shaders/hlsl/clouds/lookdev_clouds_{vs,ps}.hlsl, core/lookdev_bill_vs.hlsl, sky/sky_moon_ps.hlsl.
 * Red WW_VKVIEW_SKYBODY_RED=1: the bodies on GL (the sky's own crossing at skyRun). Gate: tests/vkframe/skybody.sh. */

#include "features/vkframe/vkpresent.h"

#include <cstdint>
#include <string>

namespace WwVkPresent
{
// the four LooseUniforms blocks, spirv-cross --reflect of the shipped SPIR-V (notes/vkfar2/tools/reflect_g2.sh)
// kT_skyStarsVs: 4 fields, 32 bytes (b31 of skystars_view_vs, at binding 32)
inline constexpr FxField kT_skyStarsVs[] = {
	{ "uvOffset", 0, 2, 1, 0, 0 }, { "probe", 8, 1, 1, 1, 0 }, { "probeUV", 16, 2, 1, 0, 0 },
	{ "uvOffset2", 24, 2, 1, 0, 0 },
};
inline constexpr int kT_skyStarsVsBytes = 32;
// kT_skyStarsPs: 27 fields, 252 bytes
inline constexpr FxField kT_skyStarsPs[] = {
	{ "atRay", 0, 3, 1, 0, 0 }, { "atMieS", 12, 1, 1, 0, 0 }, { "atMieE", 16, 1, 1, 0, 0 }, { "atHaze", 20, 3, 1, 0, 0 },
	{ "atOzone", 32, 3, 1, 0, 0 }, { "atG", 44, 1, 1, 0, 0 }, { "atHazeAlbedo", 48, 3, 1, 0, 0 },
	{ "atHazeSpec", 64, 3, 1, 0, 0 }, { "atMsOld", 76, 1, 1, 0, 0 }, { "texBlend", 80, 1, 1, 0, 0 },
	{ "colLin", 84, 3, 1, 0, 0 }, { "alpha", 96, 1, 1, 0, 0 }, { "skyScale", 100, 1, 1, 0, 0 },
	{ "probe", 104, 1, 1, 1, 0 }, { "sceneExposure", 108, 1, 1, 0, 0 }, { "viewTransform", 112, 1, 1, 1, 0 },
	{ "ldLtOn", 116, 1, 1, 1, 0 }, { "ldLtSrc", 128, 4, 1, 0, 0 }, { "ldLtPrm", 144, 4, 1, 0, 0 },
	{ "ldLtCol", 160, 3, 1, 0, 0 }, { "ldLtCam", 176, 4, 1, 0, 0 }, { "ldApOn", 192, 1, 1, 1, 0 },
	{ "ldApR", 196, 2, 1, 0, 0 }, { "ldFsOn", 204, 1, 1, 1, 0 }, { "ldFsSun", 208, 4, 1, 0, 0 },
	{ "ldFsRb", 224, 4, 1, 0, 0 }, { "ldFsTint", 240, 3, 1, 0, 0 },
};
inline constexpr int kT_skyStarsPsBytes = 256;
// kT_skyMoonVs: 2 fields, 16 bytes (b31 of skymoon_view_vs, at binding 32)
inline constexpr FxField kT_skyMoonVs[] = {
	{ "centerWorld", 0, 3, 1, 0, 0 }, { "halfSize", 12, 1, 1, 0, 0 },
};
inline constexpr int kT_skyMoonVsBytes = 16;
// kT_skyMoonPs: 33 fields, 556 bytes
inline constexpr FxField kT_skyMoonPs[] = {
	{ "atRay", 0, 3, 1, 0, 0 }, { "atMieS", 12, 1, 1, 0, 0 }, { "atMieE", 16, 1, 1, 0, 0 }, { "atHaze", 20, 3, 1, 0, 0 },
	{ "atOzone", 32, 3, 1, 0, 0 }, { "atG", 44, 1, 1, 0, 0 }, { "atHazeAlbedo", 48, 3, 1, 0, 0 },
	{ "atHazeSpec", 64, 3, 1, 0, 0 }, { "atMsOld", 76, 1, 1, 0, 0 }, { "atViewSize", 80, 2, 1, 0, 0 },
	{ "atRect", 96, 4, 1, 0, 0 }, { "atCamR", 112, 1, 1, 0, 0 }, { "atSunW", 116, 3, 1, 0, 0 },
	{ "atMoonW", 128, 3, 1, 0, 0 }, { "atSunK", 140, 1, 1, 0, 0 }, { "atMoonK", 144, 1, 1, 0, 0 },
	{ "atTintOn", 148, 1, 1, 0, 0 }, { "atTint", 160, 3, 17, 0, 0 }, { "atRecNight", 432, 2, 1, 0, 0 },
	{ "atDomeAir", 440, 1, 1, 0, 0 }, { "moonD", 448, 3, 1, 0, 0 }, { "moonS", 464, 3, 1, 0, 0 },
	{ "bodyX", 480, 3, 1, 0, 0 }, { "bodyY", 496, 3, 1, 0, 0 }, { "bodyZ", 512, 3, 1, 0, 0 }, { "sinR", 524, 1, 1, 0, 0 },
	{ "pxAng", 528, 1, 1, 0, 0 }, { "albedoInv", 532, 1, 1, 0, 0 }, { "sunTerm", 536, 1, 1, 0, 0 },
	{ "earthTerm", 540, 1, 1, 0, 0 }, { "alpha", 544, 1, 1, 0, 0 }, { "sceneExposure", 548, 1, 1, 0, 0 },
	{ "viewTransform", 552, 1, 1, 1, 0 },
};
inline constexpr int kT_skyMoonPsBytes = 560;

constexpr int kSkyStarsTex = 3;  //!< Tex, Tex2, ldTrans (t0..t2 at 64..66)
constexpr int kSkyMoonTex = 5;   //!< atAlbedo, atRelief, atTransTex, atSunView, atMoonView (t0..t4 at 64..68)

struct SkyStars
{
	int nv = 0, ni = 0;                      //!< vertices (at most 65536: 16-bit indices), indices (triangles)
	const float * pos = nullptr;             //!< nv x 3, turned on the CPU as GL draws them
	const float * col = nullptr;             //!< nv x 4, with the air's extinction when GL takes it
	const float * uv = nullptr;              //!< nv x 2
	const std::uint16_t * idx = nullptr;
	unsigned tex[kSkyStarsTex] = {};         //!< GL's 2D names on the samplers' units (0: unbound, reads 0,0,0,1)
	int vp[4] = {};                          //!< GL's viewport: the sky's, or the stars go to GL
	std::uint8_t global[kSkyGlobalBytes] = {};
	std::uint8_t vs[kT_skyStarsVsBytes] = {};
	std::uint8_t ps[kT_skyStarsPsBytes] = {};
	GlProc gp = nullptr;                     //!< the maps' sampler state (glGetTextureParameter*)
};

struct SkyMoon
{
	unsigned tex[kSkyMoonTex] = {};
	int vp[4] = {};
	std::uint8_t global[kSkyGlobalBytes] = {};
	std::uint8_t vs[kT_skyMoonVsBytes] = {};
	std::uint8_t ps[kT_skyMoonPsBytes] = {};
	GlProc gp = nullptr;
};

//! the backdrop's bodies may join the sky's pass this frame (read and cleared by the next skyRun)
void skyBodiesArm( bool on );
//! the sky's crossing is held for the bodies (skyRun deferred it this frame)
bool skyBodiesHeld();
/*! the body joins the held pass (drawn after the sky and the bodies before it). false: why, nothing joined (the caller
 * flushes, then draws on GL) */
bool skyStars( const SkyStars & s, std::string & why );
bool skyMoon( const SkyMoon & m, std::string & why );
/*! the held crossing goes now (none held: true). false: why; nothing of it drew, the sky either (the caller draws the
 * sky and the bodies that joined on GL) */
bool skyBodiesFlush( std::string & why );

/* the 2D name on a sampler's unit of the program GL is about to draw with (glGetUniformiv, GL_TEXTURE_BINDING_2D); 0 when
 * the program lacks the sampler or nothing is bound. Fn: the caller's GL functions */
template <class Fn> unsigned skyBodyUnitTex( Fn * fn, unsigned prog, const char * sampler )
{
	const int l = fn->glGetUniformLocation( prog, sampler );
	if ( l < 0 )
		return 0;
	int unit = 0, active = 0x84C0, name = 0;
	fn->glGetUniformiv( prog, l, &unit );
	fn->glGetIntegerv( 0x84E0, &active );  // GL_ACTIVE_TEXTURE
	fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
	fn->glGetIntegerv( 0x8069, &name );  // GL_TEXTURE_BINDING_2D
	fn->glActiveTexture( unsigned( active ) );
	return unsigned( name );
}
}  // namespace WwVkPresent
