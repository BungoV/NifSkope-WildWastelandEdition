#pragma once

/* Lane VKOBJ15 (2026-11-16, census row 146): the lightning strips' preview (controllers.cpp ProcLightningController::drawPreview,
 * boltstrip.prog) on the present device, into the tone frame through the fx crossing (vkbolt.inc). Plain data: the app hands
 * the CPU strip arrays, the matrices and the two texture slots; a refusal draws nothing on Vulkan (GL draws the strip).
 * Included by vkpresent.cpp's vkbolt.inc, src/gl/controllers.cpp and tests/vkframe/boltview.cpp. */
#include "features/vkframe/vkpresent.h"
#include "features/vkframe/vkobjtex.h"

#include <cstdint>
#include <string>

namespace WwVkPresent
{
// boltstrip_vs's b31 (binding 287) and boltstrip_ps's b31 (31), spirv-cross --reflect (notes/vkobj15/tools/reflect_bolt.sh)
inline constexpr FxField kT_boltVs[] = {
	{ "modelViewMatrix", 0, 4, 1, 0, 4 },
};
inline constexpr int kT_boltVsBytes = 64;
inline constexpr FxField kT_boltPs[] = {
	{ "greyscaleColor", 0, 1, 1, 1, 0 }, { "greyscaleAlpha", 4, 1, 1, 1, 0 }, { "paletteRow", 8, 1, 1, 0, 0 },
};
inline constexpr int kT_boltPsBytes = 16;

struct BoltRun
{
	int w = 0, h = 0, vp[4] = {};      //!< the tone frame's size, GL's viewport
	unsigned color = 0;               //!< the tone frame's colour (toneFrameIs)
	int n = 0;                        //!< vertices (GL_TRIANGLES)
	const float * pos = nullptr;      //!< n x 3, attribute 0
	const float * col = nullptr;      //!< n x 4, attribute 1
	const float * uv = nullptr;       //!< n x 2, attribute 2
	std::uint8_t global[kFxGlobalBytes] = {};  //!< globalUniforms (b0) as GL's block holds it (projectionMatrix at 96)
	std::uint8_t vsLoose[kT_boltVsBytes] = {};  //!< fxPack of kT_boltVs
	std::uint8_t psLoose[kT_boltPsBytes] = {};  //!< fxPack of kT_boltPs
	FxTex tex[2];                     //!< t0 boltTexture, t1 GreyscaleMap (tex 0: the zero texture)
	unsigned depthFunc = 0x0203;      //!< GL's glDepthFunc (GL_LEQUAL)
	unsigned cullFace = 0;            //!< 0 culling off, else GL's glCullFace (GL_FRONT / GL_BACK / GL_FRONT_AND_BACK)
	bool ccw = true;                  //!< GL's glFrontFace is GL_CCW
	int red = 0;                      //!< gates only: 1 no blend, 2 the palette off, 4 u and v swapped
};

/* a texture slot for boltRun: fxTexOf's reading; a texture-cache twin (objTexMake) is read with its own sampling, so
 * fxTexOf's "mipmapped sampling other than trilinear repeat" does not refuse it (a sampler object still does) */
template <class Fn> bool boltTexOf( Fn * fn, unsigned prog, const char * sampler, FxTex & t, std::string & why )
{
	std::string w;
	if ( fxTexOf( fn, prog, sampler, 0, t, w ) )
		return true;
	if ( t.tex && objTexKey( t.tex ) && w.find( "sampler object" ) == std::string::npos )
		return true;
	why = w;
	return false;
}

/* GL's state at drawPreview's draw into t (depth func, culling, front face); false names a state the Vulkan pipeline does not
 * mirror (drawPreview sets blend SRC_ALPHA / ONE, depth test on, depth mask off; the rest is the scene's) */
template <class Fn> bool boltStateOf( Fn * fn, BoltRun & t, std::string & why )
{
	const struct
	{
		unsigned cap;
		const char * name;
	} off[7] = { { 0x0B90, "stencil test" }, { 0x0C11, "scissor test" }, { 0x809E, "alpha to coverage" }, { 0x8037, "polygon offset fill" },
		{ 0x8C36, "sample shading" }, { 0x864F, "depth clamp" }, { 0x8E51, "sample mask" } };
	for ( const auto & o : off )
		if ( fn->glIsEnabled( o.cap ) ) {
			why = std::string( "GL's " ) + o.name + " is on";
			return false;
		}
	if ( !fn->glIsEnabled( 0x0BE2 ) || !fn->glIsEnabled( 0x0B71 ) ) {
		why = "GL's blend or depth test is off";
		return false;
	}
	int v[6] = {};
	fn->glGetIntegerv( 0x80C9, &v[0] );  // GL_BLEND_SRC_RGB
	fn->glGetIntegerv( 0x80C8, &v[1] );  // GL_BLEND_DST_RGB
	fn->glGetIntegerv( 0x80CB, &v[2] );  // GL_BLEND_SRC_ALPHA
	fn->glGetIntegerv( 0x80CA, &v[3] );  // GL_BLEND_DST_ALPHA
	fn->glGetIntegerv( 0x8009, &v[4] );  // GL_BLEND_EQUATION_RGB
	fn->glGetIntegerv( 0x883D, &v[5] );  // GL_BLEND_EQUATION_ALPHA
	if ( v[0] != 0x0302 || v[1] != 1 || v[2] != 0x0302 || v[3] != 1 || v[4] != 0x8006 || v[5] != 0x8006 ) {
		why = "GL's blend is not SRC_ALPHA / ONE, ADD";
		return false;
	}
	unsigned char m[4] = {}, dm = 1;
	fn->glGetBooleanv( 0x0C23, m );  // GL_COLOR_WRITEMASK
	fn->glGetBooleanv( 0x0B72, &dm );  // GL_DEPTH_WRITEMASK
	if ( !m[0] || !m[1] || !m[2] || !m[3] || dm ) {
		why = "GL's colour mask is not all or its depth mask is on";
		return false;
	}
	int df = 0, cf = 0, ff = 0;
	fn->glGetIntegerv( 0x0B74, &df );  // GL_DEPTH_FUNC
	fn->glGetIntegerv( 0x0B45, &cf );  // GL_CULL_FACE_MODE
	fn->glGetIntegerv( 0x0B46, &ff );  // GL_FRONT_FACE
	t.depthFunc = unsigned( df );
	t.cullFace = fn->glIsEnabled( 0x0B44 ) ? unsigned( cf ) : 0u;
	t.ccw = ff == 0x0901;
	return true;
}

/* GL current, the tone frame bound, boltstrip's uniforms set: the strip on Vulkan (blend SRC_ALPHA / ONE, depth test with
 * GL's func, no depth write, no stencil, as drawPreview's). false: why; nothing drawn (GL draws its strip) */
bool boltRun( const BoltRun & t, std::string & why );
void boltOnGl( const char * why );
FxStat takeBoltStat();
}  // namespace WwVkPresent
